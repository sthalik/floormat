#include "app.hpp"
#include "compat/round-to-even.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include "loader/loader.hpp"
#include "serialize/json-wrapper.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/object.hpp"
#include "src/hole.hpp"
#include "src/scenery-proto.hpp"
#include "loader/scenery-cell.hpp"
#include "src/wall-atlas.hpp"
#include "loader/wall-cell.hpp"
#include "src/tile-image.hpp"
#include "src/tile-defs.hpp"
#include "src/global-coords.hpp"
#include "src/collision.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-pass-pool.hpp"
#include "src/search.hpp"
#include "src/search-pred.hpp"
#include <utility>
#include <mg/Range.h>

namespace floormat {
namespace {

void test_round_to_even_self()
{
    fm_assert(round_to_even<uint32_t>(0,   0)   == 0);
    fm_assert(round_to_even<uint32_t>(1,   1)   == 2);
    fm_assert(round_to_even<uint32_t>(2,   2)   == 2);
    fm_assert(round_to_even<uint32_t>(3,   3)   == 4);
    fm_assert(round_to_even<uint32_t>(4,   4)   == 4);
    fm_assert(round_to_even<uint32_t>(5,   5)   == 6);
    fm_assert(round_to_even<uint32_t>(254, 254) == 254);
    fm_assert(round_to_even<uint8_t> (1,   1)   == 2);
    fm_assert(round_to_even<uint8_t> (0xfe, 0xfe) == 0xfe);
    fm_assert(round_to_even<uint8_t> (0xff, 0xff) == 0xfe);

    fm_assert(round_to_even<uint32_t>(1, 3) == 0);
    fm_assert(round_to_even<uint32_t>(3, 5) == 2);
    fm_assert(round_to_even<uint32_t>(5, 3) == 6);
}

void test_pool_for_promotes_and_snaps()
{
    Grid::Pass::PoolRegistry reg{(uint32_t)tile_size_xy / 4};

    auto& p2 = reg.pool_for(2);
    fm_assert(p2.params().bbox_size == Grid::Pass::Grid::min_bbox_size);
    fm_assert(p2.params().bbox_size == 4);

    auto& p4 = reg.pool_for(4);
    fm_assert(p4.params().bbox_size == 4);
    fm_assert(&p2 == &p4);

    auto& p8 = reg.pool_for(8);
    fm_assert(p8.params().bbox_size == 8);
    fm_assert(&p8 != &p4);

    auto& p5 = reg.pool_for(5);
    fm_assert(p5.params().bbox_size == 6);

    auto& p64 = reg.pool_for((uint32_t)tile_size_xy);
    fm_assert(p64.params().bbox_size == (uint32_t)tile_size_xy);
}

void test_const_passable_equivalence()
{
    const auto wall_atlas = loader.invalid_wall_atlas().atlas;

    auto w = world();
    constexpr chunk_coords_ ch{0, 0, 0};
    auto& c = w[ch];
    c.mark_modified();
    c[{4, 4}].wall_north() = { wall_atlas, 0 };
    c.ensure_passability();

    constexpr Vector2 q_blocked_min{4*tile_size_xy - 8.f,  4*tile_size_xy - 40.f};
    constexpr Vector2 q_blocked_max{4*tile_size_xy + 8.f,  4*tile_size_xy - 24.f};
    constexpr Vector2 q_clear_min  {8*tile_size_xy + 0.f,  8*tile_size_xy + 0.f};
    constexpr Vector2 q_clear_max  {8*tile_size_xy + 16.f, 8*tile_size_xy + 16.f};

    auto pred_nc = [](chunk&,        collision_data, Range2D) { return path_search_continue::blocked; };
    auto pred_cc = [](const chunk&,  collision_data, Range2D) { return path_search_continue::blocked; };

    bool nonc_blk = Search::is_passable_1(c, q_blocked_min, q_blocked_max, pred_nc);
    bool nonc_ok  = Search::is_passable_1(c, q_clear_min,   q_clear_max,   pred_nc);
    bool consc_blk = Search::is_passable_1(std::as_const(c), q_blocked_min, q_blocked_max, pred_cc);
    bool consc_ok  = Search::is_passable_1(std::as_const(c), q_clear_min,   q_clear_max,   pred_cc);

    fm_assert(!nonc_blk);
    fm_assert(nonc_ok);
    fm_assert(nonc_blk == consc_blk);
    fm_assert(nonc_ok  == consc_ok);

    const auto bb_blocked = Range2D{q_blocked_min, q_blocked_max};
    const auto bb_clear   = Range2D{q_clear_min,   q_clear_max};
    bool nw_blk = Search::is_passable(w,                ch, bb_blocked, pred_nc);
    bool nw_ok  = Search::is_passable(w,                ch, bb_clear,   pred_nc);
    bool cw_blk = Search::is_passable(std::as_const(w), ch, bb_blocked, pred_cc);
    bool cw_ok  = Search::is_passable(std::as_const(w), ch, bb_clear,   pred_cc);
    fm_assert(nw_blk == cw_blk);
    fm_assert(nw_ok  == cw_ok);
    fm_assert(!nw_blk);
    fm_assert(nw_ok);
}

void test_const_passable_with_neighbors()
{
    const auto wall_atlas = loader.invalid_wall_atlas().atlas;
    auto w = world();

    constexpr chunk_coords_ ch_self{0, 0, 0}, ch_east{1, 0, 0}, ch_south{0, 1, 0};
    auto& cs = w[ch_self]; auto& ce = w[ch_east]; auto& cS = w[ch_south];
    cs.mark_modified(); ce.mark_modified(); cS.mark_modified();
    ce[{0, 8}].wall_west() = { wall_atlas, 0 };
    cs.ensure_passability(); ce.ensure_passability(); cS.ensure_passability();

    constexpr Vector2 q_min{15*tile_size_xy + 0.f,   8*tile_size_xy - 16.f};
    constexpr Vector2 q_max{16*tile_size_xy + 16.f,  8*tile_size_xy + 16.f};

    int n_mut = 0, n_const = 0;
    auto pred_nc = [&](chunk&,       collision_data, Range2D) { ++n_mut;   return path_search_continue::blocked; };
    auto pred_cc = [&](const chunk&, collision_data, Range2D) { ++n_const; return path_search_continue::blocked; };

    bool m = Search::is_passable(w,                ch_self, Range2D{q_min, q_max}, pred_nc);
    bool c = Search::is_passable(std::as_const(w), ch_self, Range2D{q_min, q_max}, pred_cc);
    fm_assert(m == c);
    fm_assert(n_mut == n_const);
    fm_assert(n_mut >= 1);
}

void test_pred_rect_is_in_self_chunk_frame()
{
    const auto wall_atlas = loader.invalid_wall_atlas().atlas;
    auto w = world();
    constexpr chunk_coords_ ch_self{0, 0, 0};
    constexpr chunk_coords_ ch_east{1, 0, 0};

    auto& cs = w[ch_self];
    auto& ce = w[ch_east];
    cs.mark_modified();
    ce.mark_modified();
    ce[{0, 8}].wall_west() = { wall_atlas, 0 };

    constexpr Vector2 q_min{15*tile_size_xy + 0.f,   8*tile_size_xy - 16.f};
    constexpr Vector2 q_max{16*tile_size_xy + 16.f,  8*tile_size_xy + 16.f};

    chunk* captured_self = nullptr;
    Range2D captured_rect{};
    int n_pred_calls = 0;

    auto pred = [&](chunk& self, collision_data, Range2D rect) {
        captured_self = &self;
        captured_rect = rect;
        ++n_pred_calls;
        return path_search_continue::blocked;
    };

    bool ok = Search::is_passable(w, ch_self, Range2D{q_min, q_max}, pred);
    fm_assert(!ok);
    fm_assert(n_pred_calls >= 1);
    fm_assert(captured_self == &ce);

    fm_assert(captured_rect.min().x() < (float)tile_size_xy);
    fm_assert(captured_rect.max().x() < (float)tile_size_xy);
    fm_assert(captured_rect.min().y() >= (float)(7*tile_size_xy));
    fm_assert(captured_rect.max().y() <= (float)(9*tile_size_xy));
}

void test_chunk_bounds_early_out()
{
    auto w = world();
    constexpr chunk_coords_ ch{0, 0, 0};
    auto& c = w[ch];
    c.mark_modified();
    c.ensure_passability();

    int n_pred_calls = 0;
    auto pred = [&](chunk&, collision_data, Range2D) {
        ++n_pred_calls;
        return path_search_continue::blocked;
    };

    constexpr Vector2 far_min{2000.f, 2000.f}, far_max{2010.f, 2010.f};
    fm_assert(Search::is_passable_1(c, far_min, far_max, pred));
    fm_assert(n_pred_calls == 0);

    constexpr Vector2 nw_min{-2000.f, -2000.f}, nw_max{-1990.f, -1990.f};
    fm_assert(Search::is_passable_1(c, nw_min, nw_max, pred));
    fm_assert(n_pred_calls == 0);
}

void test_hole_zero_bbox_skipped_at_build()
{
    auto w = world();
    constexpr chunk_coords_ ch{0, 0, 0};
    auto& c = w[ch];

    hole_proto p;
    p.bbox_size = Vector2ub{0, 0};
    p.pass = pass_mode::pass;

    auto h = w.make_object<hole>(w.make_id(), {ch, {4, 4}}, p);
    fm_assert(h);
    fm_assert(h->bbox_size == Vector2ub{0, 0});
    fm_assert(h->flags.enabled);

    c.mark_passability_modified();
    c.ensure_passability();
    fm_assert(!c.is_passability_modified());
}

void test_can_move_to_zero_bbox()
{
    auto w = world();
    constexpr chunk_coords_ ch{0, 0, 0};
    auto& c = w[ch];
    (void)c;

    hole_proto p;
    p.bbox_size = Vector2ub{0, 0};
    p.pass = pass_mode::pass;

    auto h = w.make_object<hole>(w.make_id(), {ch, {4, 4}}, p);
    fm_assert(h);
    fm_assert(h->can_move_to(Vector2i{}));
    fm_assert(h->can_move_to(Vector2i{1, 0}));
}

} // namespace

void Test::test_passability_bbox()
{
    test_round_to_even_self();
    test_pool_for_promotes_and_snaps();
    test_const_passable_equivalence();
    test_const_passable_with_neighbors();
    test_pred_rect_is_in_self_chunk_frame();
    test_chunk_bounds_early_out();
    test_hole_zero_bbox_skipped_at_build();
    test_can_move_to_zero_bbox();
}

} // namespace floormat
