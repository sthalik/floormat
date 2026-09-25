#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/debug.hpp"
#include "compat/function2.hpp"
#include "src/grid-pass.hpp"
#include "src/hole.hpp"
#include "src/RTree.hpp"
#include "src/search.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/scenery-proto.hpp"
#include "src/tile-defs.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat::Test {

namespace {

constexpr auto chunk_size_xy = tile_size_xy * (int32_t)TILE_MAX_DIM;

constexpr chunk_coords_ COORD{0, 0, 0};
constexpr chunk_coords_ COORD_E{1, 0, 0};
constexpr chunk_coords_ COORD_Eʹ{0, 1, 0};

tile_image_proto floor_proto()
{
    return tile_image_proto{ loader.ground_atlas("tiles"), 0 };
}

wall_image_proto wall_proto()
{
    return wall_image_proto{ loader.wall_atlas("empty"), 0 };
}

void add_ground_all(chunk& c)
{
    const auto floor = floor_proto();
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = floor;
}

void clear_ground_all(chunk& c)
{
    const auto empty = tile_image_proto{};
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = empty;
}

void add_wall_north(chunk& c, local_coords pos)
{
    c[pos].wall_north() = wall_proto();
}

void rebuild_passability(chunk& c)
{
    c.mark_passability_modified();
    c.ensure_passability();
}

void tick(world& w, Pass::Pool& pool)
{
    const auto frame = w.frame_no();
    pool.maybe_mark_stale_all(frame);
    pool.build_if_stale_all(Search::without_critters());
}

uint32_t count_passable(const Pass::Grid& g)
{
    const auto dc = g.div_count();
    uint32_t n = 0;
    for (auto j = 0u; j < dc; j++)
        for (auto i = 0u; i < dc; i++)
            if (g.bit(Pass::Grid::get_bitmask_index(i, j, dc)))
                n++;
    return n;
}

uint32_t count_passable_built(Pass::Pool& pool, chunk& c)
{
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());
    return count_passable(g);
}

void test_ground_only_is_fully_passable(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);

    Pass::Grid grid = pool[c];
    grid.build_if_stale(Search::without_critters());

    const auto dc = grid.div_count();
    fm_assert(dc == (uint32_t)chunk_size_xy / div_size);
    fm_assert(count_passable(grid) == dc * dc);
}

void test_wall_blocks_some_cells(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    {
        Pass::Grid grid = pool[c];
        grid.build_if_stale(Search::without_critters());
        const auto dc = grid.div_count();
        fm_assert(count_passable(grid) == dc * dc);
    }

    add_wall_north(c, {8, 8});
    rebuild_passability(c);

    tick(w, pool);
    {
        Pass::Grid grid = pool[c];
        grid.build_if_stale(Search::without_critters());
        const auto dc = grid.div_count();
        const auto after = count_passable(grid);
        fm_assert(after < dc * dc);
        fm_assert(after > 0);
    }
}

void test_idempotent_rebuild(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid grid = pool[c];
    grid.build_if_stale(Search::without_critters());
    const auto first = count_passable(grid);

    grid.mark_stale();
    grid.build_if_stale(Search::without_critters());
    const auto second = count_passable(grid);
    fm_assert(first == second);
}

void test_neighbor_cascade(uint32_t div_size)
{
    auto w = world();
    auto& c0 = w[COORD];
    auto& c1 = w[COORD_E];
    add_ground_all(c0);
    add_ground_all(c1);
    rebuild_passability(c0);
    rebuild_passability(c1);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g0 = pool[c0];
    Pass::Grid g1 = pool[c1];
    g0.build_if_stale(Search::without_critters());
    g1.build_if_stale(Search::without_critters());
    const auto base1 = count_passable(g1);

    add_wall_north(c1, {0, 0});
    rebuild_passability(c1);

    tick(w, pool);
    Pass::Grid g1ʹ = pool[c1];
    const auto after1 = count_passable(g1ʹ);
    fm_assert(after1 < base1);
}

void test_collect_cleanup(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    {
        Pass::Grid grid = pool[c];
        grid.build_if_stale(Search::without_critters());
        fm_assert(count_passable(grid) > 0);
    }

    fm_assert(w.at(COORD) != nullptr);

    clear_ground_all(c);
    w.collect(true, true);
    fm_assert(w.at(COORD) == nullptr);

    tick(w, pool);
}

void test_add_remove_wall_restores_baseline(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto baseline = count_passable_built(pool, c);

    add_wall_north(c, {8, 8});
    rebuild_passability(c);
    tick(w, pool);
    const auto with_wall = count_passable_built(pool, c);
    fm_assert(with_wall < baseline);

    c[{8, 8}].wall_north() = wall_image_proto{};
    rebuild_passability(c);
    tick(w, pool);
    const auto restored = count_passable_built(pool, c);
    fm_assert(restored == baseline);
}

void test_neighbor_west_wall_affects_east_edge(uint32_t div_size)
{
    auto w = world();
    auto& c0 = w[COORD];
    auto& c1 = w[COORD_E];
    add_ground_all(c0);
    add_ground_all(c1);
    rebuild_passability(c0);
    rebuild_passability(c1);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto base0 = count_passable_built(pool, c0);

    for (auto y = 0u; y < TILE_MAX_DIM; y++)
        c1[{0, (uint8_t)y}].wall_west() = wall_proto();
    rebuild_passability(c1);

    tick(w, pool);
    const auto after0 = count_passable_built(pool, c0);
    fm_assert(after0 < base0);
}

void test_bit_from_tile_center_passable(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid grid = pool[c];
    grid.build_if_stale(Search::without_critters());

    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
        {
            const auto idx = grid.get_bitmask_index_from_coord(local_coords{(uint8_t)i, (uint8_t)j}, Vector2b{0, 0});
            fm_assert(grid.bit(idx));
        }
}

void test_wall_then_clear_ground_allows_collect(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    { const auto passable = count_passable_built(pool, c);
      fm_assert(passable > 0); }

    clear_ground_all(c);
    c[{4, 4}].wall_north() = wall_image_proto{};
    w.collect(true, true);
    fm_assert(w.at(COORD) == nullptr);

    tick(w, pool);
}

void test_cell_at_wall_is_blocked(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {8, 8});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());

    const auto idx = g.get_bitmask_index_from_coord(local_coords{8, 7}, Vector2b{0, 28});
    fm_assert(!g.bit(idx));
}

void test_cell_south_of_wall_is_passable(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {8, 8});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());

    const auto idx = g.get_bitmask_index_from_coord(local_coords{8, 10}, Vector2b{0, 0});
    fm_assert(g.bit(idx));
}

void test_all_chunk_corners_passable(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());

    const local_coords corners[] = { {0, 0}, {15, 0}, {0, 15}, {15, 15} };
    for (const auto lc : corners)
    {
        const auto idx = g.get_bitmask_index_from_coord(lc, Vector2b{0, 0});
        fm_assert(g.bit(idx));
    }
}

void test_div_count_derived_from_div_size(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());

    fm_assert(g.div_count() == (uint32_t)chunk_size_xy / div_size);
    fm_assert(g.div_count() * div_size == (uint32_t)chunk_size_xy);
}

void test_pool_destruction_with_live_grids()
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    {
        Pass::Pool pool{Pass::Params{16}};
        tick(w, pool);
        Pass::Grid g = pool[c];
        g.build_if_stale(Search::without_critters());
        fm_assert(count_passable(g) > 0);
    }
}

void add_blocked_ground_all(chunk& c)
{
    const auto blocker = tile_image_proto{ loader.ground_atlas("texel"), 0 };
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = blocker;
}

void test_blocked_ground_blocks_all(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_blocked_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());
    fm_assert(count_passable(g) == 0);
}

void test_multi_chunk_3x3_sanity(uint32_t div_size)
{
    auto w = world();
    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
        {
            auto& c = w[{(int16_t)x, (int16_t)y, 0}];
            add_ground_all(c);
            rebuild_passability(c);
        }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);

    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
        {
            auto& c = *w.at(chunk_coords_{(int16_t)x, (int16_t)y, 0});
            Pass::Grid g = pool[c];
            g.build_if_stale(Search::without_critters());
            const auto dc = g.div_count();
            fm_assert(count_passable(g) == dc * dc);
        }
}

void test_chunk_reinsertion_after_collect(uint32_t div_size)
{
    auto w = world();

    {
        auto& c = w[COORD];
        add_ground_all(c);
        rebuild_passability(c);
    }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    { Pass::Grid g = pool[*w.at(COORD)];
      g.build_if_stale(Search::without_critters());
      fm_assert(count_passable(g) > 0); }

    clear_ground_all(*w.at(COORD));
    w.collect(true, true);
    fm_assert(w.at(COORD) == nullptr);

    tick(w, pool);

    auto& c2 = w[COORD];
    add_ground_all(c2);
    rebuild_passability(c2);

    tick(w, pool);
    Pass::Grid g2 = pool[c2];
    g2.build_if_stale(Search::without_critters());
    const auto dc = g2.div_count();
    fm_assert(count_passable(g2) == dc * dc);
}

void test_chunk_pass_gen_unique_after_collect()
{
    auto w = world();

    auto id1 = w[COORD].pass_gen();
    fm_assert(id1 != 0);

    w.collect(true, true);
    fm_assert(w.at(COORD) == nullptr);

    auto id2 = w[COORD].pass_gen();
    fm_assert(id2 != 0);
    fm_assert(id1 != id2);
}

void make_chunk_row(world& w, int16_t n)
{
    for (int16_t i = 0; i < n; i++)
        w[chunk_coords_{i, 0, 0}];
}

uint64_t max_pass_gen(world& w)
{
    uint64_t ret = 0;
    for (const auto& c : w.chunks())
        ret = Math::max(ret, c.pass_gen());
    return ret;
}

void test_chunk_pass_gen_monotonic_after_move_ctor()
{
    auto a = world();
    make_chunk_row(a, 20);
    auto b = world(move(a));
    const auto old = max_pass_gen(b);
    auto& c = b[COORD];
    c.mark_passability_modified();
    fm_assert(c.pass_gen() > old);
}

void test_chunk_pass_gen_monotonic_after_move_assign()
{
    auto a = world();
    make_chunk_row(a, 20);
    auto b = world();
    b[COORD];
    b = move(a);
    const auto old = max_pass_gen(b);
    auto& c = b[COORD];
    c.mark_passability_modified();
    fm_assert(c.pass_gen() > old);
}

void test_frame_counter_ticks_independently(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    { Pass::Grid g = pool[c];
      g.build_if_stale(Search::without_critters());
      (void)count_passable(g); }

    (void)w.increment_frame_no();
    tick(w, pool);

    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());
    const auto dc = g.div_count();
    fm_assert(count_passable(g) == dc * dc);
}

void test_empty_pool_safe_tick(uint32_t div_size)
{
    auto w = world();
    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    tick(w, pool);
}

void test_last_bit_index_valid(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());

    const auto dc = g.div_count();
    const auto last_idx = Pass::Grid::get_bitmask_index(dc - 1, dc - 1, dc);
    fm_assert(g.bit(last_idx));
}

void test_wall_west_blocks(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    c[{8, 8}].wall_west() = wall_proto();
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());

    const auto dc = g.div_count();
    fm_assert(count_passable(g) < dc * dc);
    fm_assert(count_passable(g) > 0);
}

void test_many_walls_strictly_reduce_passable(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto base = count_passable_built(pool, c);

    add_wall_north(c, {4, 4});
    rebuild_passability(c);
    tick(w, pool);
    const auto one = count_passable_built(pool, c);
    fm_assert(one < base);

    add_wall_north(c, {8, 8});
    rebuild_passability(c);
    tick(w, pool);
    const auto two = count_passable_built(pool, c);
    fm_assert(two < one);
}

void test_repeated_ticks_noop_on_stable_world(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {5, 5});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto first = count_passable_built(pool, c);

    for (int i = 0; i < 5; i++)
    {
        tick(w, pool);
        fm_assert(count_passable_built(pool, c) == first);
    }
}

void test_explicit_wrapper_mark_stale_rebuilds(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto base = count_passable_built(pool, c);

    Pass::Grid g = pool[c];
    g.mark_stale();
    g.build_if_stale(Search::without_critters());
    fm_assert(count_passable(g) == base);
}

void test_mark_modified_only_without_ensure_passability(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto base = count_passable_built(pool, c);

    add_wall_north(c, {8, 8});
    c.mark_passability_modified();

    tick(w, pool);
    const auto after = count_passable_built(pool, c);
    fm_assert(after < base);
}

void test_neighbor_mark_only_without_ensure_detected(uint32_t div_size)
{
    auto w = world();
    auto& c0 = w[COORD];
    auto& c1 = w[COORD_E];
    add_ground_all(c0);
    add_ground_all(c1);
    rebuild_passability(c0);
    rebuild_passability(c1);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto base0 = count_passable_built(pool, c0);

    for (auto y = 0u; y < TILE_MAX_DIM; y++)
        c1[{0, (uint8_t)y}].wall_west() = wall_proto();
    c1.mark_passability_modified();

    tick(w, pool);
    const auto after0 = count_passable_built(pool, c0);
    fm_assert(after0 < base0);
}

void test_params_preserved_through_pool(uint32_t div_size)
{
    Pass::Pool pool{Pass::Params{div_size, tile_size_xy}};
    const auto p = pool.params();
    fm_assert(p.div_size == div_size);
    fm_assert(p.bbox_size == tile_size_xy);
}

void test_collect_all_chunks_no_crash(uint32_t div_size)
{
    auto w = world();
    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
        {
            auto& c = w[{(int16_t)x, (int16_t)y, 0}];
            add_ground_all(c);
            rebuild_passability(c);
        }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);

    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
            { auto& c = *w.at(chunk_coords_{(int16_t)x, (int16_t)y, 0});
              (void)count_passable_built(pool, c); }

    for (auto y = -1; y <= 1; y++)
        for (auto x = -1; x <= 1; x++)
            clear_ground_all(*w.at(chunk_coords_{(int16_t)x, (int16_t)y, 0}));

    w.collect(true, true);
    fm_assert(w.size() == 0);

    tick(w, pool);
}

void test_pool_reuse_after_collect(uint32_t div_size)
{
    constexpr chunk_coords_ C11{1, 1, 0};
    auto w = world();
    auto& c = w[C11];
    add_wall_north(c, {0, 0});
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    { Pass::Grid g = pool[c]; g.build_if_stale(Search::without_critters()); }

    const auto before = pool.pooled_count();

    c[{0, 0}].wall_north() = wall_image_proto{};
    c.mark_passability_modified();
    w.collect(true, true);
    fm_assert(w.at(C11) == nullptr);

    pool.maybe_mark_stale_all(w.frame_no());
    fm_assert(pool.pooled_count() == before + 1);
}

void test_pool_reuse_neighbors_collected(uint32_t div_size)
{
    constexpr chunk_coords_ A{1, 1, 0};
    constexpr chunk_coords_ B{2, 1, 0};
    auto w = world();
    { auto& ca = w[A]; add_wall_north(ca, {0, 0}); rebuild_passability(ca); }
    { auto& cb = w[B]; add_wall_north(cb, {0, 0}); rebuild_passability(cb); }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    { (void)pool[*w.at(A)]; (void)pool[*w.at(B)]; }

    const auto before = pool.pooled_count();

    { auto& ca = *w.at(A);
      ca[{0, 0}].wall_north() = wall_image_proto{};
      ca.mark_passability_modified(); }
    { auto& cb = *w.at(B);
      cb[{0, 0}].wall_north() = wall_image_proto{};
      cb.mark_passability_modified(); }
    w.collect(true, true);
    fm_assert(w.at(A) == nullptr);
    fm_assert(w.at(B) == nullptr);

    pool.maybe_mark_stale_all(w.frame_no());
    fm_assert(pool.pooled_count() == before + 2);
}

void test_neighbor_lifecycle(uint32_t div_size)
{
    constexpr chunk_coords_ A{0, 0, 0};
    constexpr chunk_coords_ B{1, 0, 0};
    auto w = world();
    { auto& ca = w[A]; add_ground_all(ca); rebuild_passability(ca); }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);

    { auto& cb = w[B]; add_ground_all(cb); rebuild_passability(cb); }
    tick(w, pool);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);
    fm_assert(count_passable_built(pool, *w.at(B)) > 0);

    const auto pooled0 = pool.pooled_count();
    clear_ground_all(*w.at(B));
    w.collect(true, true);
    fm_assert(w.at(B) == nullptr);
    tick(w, pool);
    fm_assert(pool.pooled_count() == pooled0 + 1);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);

    const auto pooled1 = pool.pooled_count();
    { auto& cb = w[B]; add_ground_all(cb); rebuild_passability(cb); }
    tick(w, pool);
    fm_assert(count_passable_built(pool, *w.at(B)) > 0);
    fm_assert(pool.pooled_count() == pooled1 - 1);
    fm_assert(count_passable_built(pool, *w.at(A)) > 0);
}

void test_self_collect_with_neighbor(uint32_t div_size)
{
    constexpr chunk_coords_ A{0, 0, 0};
    constexpr chunk_coords_ B{1, 0, 0};
    auto w = world();
    { auto& ca = w[A]; add_ground_all(ca); rebuild_passability(ca); }
    { auto& cb = w[B]; add_ground_all(cb); rebuild_passability(cb); }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    (void)count_passable_built(pool, *w.at(A));
    (void)count_passable_built(pool, *w.at(B));

    const auto pooled0 = pool.pooled_count();
    clear_ground_all(*w.at(A));
    w.collect(true, true);
    fm_assert(w.at(A) == nullptr);
    fm_assert(w.at(B) != nullptr);

    tick(w, pool);
    fm_assert(pool.pooled_count() == pooled0 + 1);
    fm_assert(count_passable_built(pool, *w.at(B)) > 0);
}

void test_pool_reuse_across_different_coord(uint32_t div_size)
{
    constexpr chunk_coords_ X{0, 0, 0};
    constexpr chunk_coords_ Y{3, 3, 0};
    auto w = world();
    { auto& c = w[X]; add_ground_all(c); rebuild_passability(c); }

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    (void)count_passable_built(pool, *w.at(X));

    clear_ground_all(*w.at(X));
    w.collect(true, true);
    tick(w, pool);
    const auto pooled_after_collect = pool.pooled_count();
    fm_assert(pooled_after_collect >= 1);

    { auto& c = w[Y]; add_ground_all(c); rebuild_passability(c); }
    tick(w, pool);
    (void)count_passable_built(pool, *w.at(Y));
    fm_assert(pool.pooled_count() == pooled_after_collect - 1);
}

void test_mark_and_ensure_preserve_frame_no(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    const auto frame = w.frame_no();

    add_wall_north(c, {4, 4});
    c.mark_passability_modified();
    fm_assert(w.frame_no() == frame);
    c.ensure_passability();
    fm_assert(w.frame_no() == frame);

    tick(w, pool);
    fm_assert(w.frame_no() == frame);

    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());
    fm_assert(w.frame_no() == frame);
}

void test_multiple_pooled_items(uint32_t div_size)
{
    auto w = world();
    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    {
        auto& c = w[COORD];
        tick(w, pool);
        fm_assert(pool.pooled_count() == 0);
        (void)pool[c];
        fm_assert(pool.pooled_count() == 0);
    }
    w.collect(true, true); // invalidates c
    tick(w, pool);
    fm_assert(pool.pooled_count() == 1);
    {
        auto& c = w[COORD];
        (void)pool[c];
        auto& cʹ = w[COORD_E];
        (void)pool[cʹ];
        fm_assert(pool.pooled_count() == 0);
    }
    w.collect(true, true); // invalidates c, cʹ
    tick(w, pool);
    fm_assert(pool.pooled_count() == 2);
    {
        auto& c = w[COORD];
        (void)pool[c];
        fm_assert(pool.pooled_count() == 1);
        auto& cʹ = w[COORD_E];
        (void)pool[cʹ];
        fm_assert(pool.pooled_count() == 0);
        auto& cʹʹ = w[COORD_Eʹ];
        (void)pool[cʹʹ];
        fm_assert(pool.pooled_count() == 0);
    }
    w.collect(true, true); // invalidates c, cʹ, cʹʹ
    tick(w, pool);
    fm_assert(pool.pooled_count() == 3);
}

void test_bitview_read_matches_bit(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    add_wall_north(c, {4, 4});
    add_wall_north(c, {8, 8});
    c[{2, 6}].wall_west() = wall_proto();
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());

    const auto dc = g.div_count();
    const auto view = g.bits();
    for (uint32_t j = 0; j < dc; j++)
        for (uint32_t i = 0; i < dc; i++)
        {
            const auto idx = Pass::Grid::get_bitmask_index(i, j, dc);
            fm_assert(view.read(idx) == g.bit(idx));
        }
}

void test_partial_collect_then_collect_survivors(uint32_t div_size)
{
    auto w = world();
    Pass::Pool pool{Pass::Params{div_size}};

    {
        auto& a = w[COORD];
        auto& b = w[COORD_E];
        auto& d = w[COORD_Eʹ];
        add_ground_all(a); add_ground_all(b); add_ground_all(d);
        rebuild_passability(a); rebuild_passability(b); rebuild_passability(d);
        tick(w, pool);
        (void)pool[a]; (void)pool[b]; (void)pool[d];
        clear_ground_all(b);
    }
    w.collect(true, true);
    fm_assert(w.at(COORD)    != nullptr);
    fm_assert(w.at(COORD_E)  == nullptr);
    fm_assert(w.at(COORD_Eʹ) != nullptr);

    tick(w, pool);
    fm_assert(pool.pooled_count() == 1);

    {
        auto& b = w[COORD_E];
        add_ground_all(b);
        rebuild_passability(b);
        tick(w, pool);
        (void)pool[b];
    }
    fm_assert(pool.pooled_count() == 0);

    {
        clear_ground_all(*w.at(COORD));
        clear_ground_all(*w.at(COORD_Eʹ));
    }
    w.collect(true, true);
    fm_assert(w.at(COORD)    == nullptr);
    fm_assert(w.at(COORD_E)  != nullptr);
    fm_assert(w.at(COORD_Eʹ) == nullptr);

    tick(w, pool);
    fm_assert(pool.pooled_count() == 2);
}

// A bit means "a bbox_size rect at any position in this cell fits".
void test_bit_matches_every_position()
{
    constexpr uint32_t div_size = 2, bbox_size = 4;
    constexpr uint8_t at_x = 4, at_y = 4;
    constexpr Vector2b obstacle_offset{2, 2};
    constexpr int obstacle_size = 2;

    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    {
        scenery_proto p;
        p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
        p.subtype   = generic_scenery_proto{};
        p.offset    = obstacle_offset;
        p.bbox_size = Vector2ub(obstacle_size);
        p.pass      = pass_mode::blocked;
        w.make_scenery(w.make_id(), {COORD, local_coords{at_x, at_y}}, move(p));
    }
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, bbox_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());
    fm_assert(pool.params().div_size == div_size);
    fm_assert(pool.params().bbox_size == bbox_size);

    const auto& pred = Search::without_critters();
    const auto dc = g.div_count();
    constexpr int d = (int)div_size, half_tile = tile_size_xy/2;
    constexpr auto hb = (float)bbox_size * .5f;
    // scenery_tile(): center - bbox_size/2, then + bbox_size.
    constexpr int m = tile_size_xy*at_x + obstacle_offset.x() - obstacle_size/2,
                  M = m + obstacle_size;
    // Only cells that can see the obstacle. A full div_count² sweep here is 512² cells.
    constexpr int lo = (m - (int)bbox_size - 2*d + half_tile) / d,
                  hi = (M + (int)bbox_size + 2*d + half_tile) / d;

    for (int j = lo; j <= hi; j++)
        for (int i = lo; i <= hi; i++)
        {
            bool passable = true;
            for (int py = j*d - half_tile; py < (j+1)*d - half_tile && passable; py++)
                for (int px = i*d - half_tile; px < (i+1)*d - half_tile && passable; px++)
                    passable = Search::is_passable_1(c, Vector2{(float)px - hb, (float)py - hb},
                                                        Vector2{(float)px + hb, (float)py + hb}, pred);
            const auto idx = Pass::Grid::get_bitmask_index((uint32_t)i, (uint32_t)j, dc);
            if (g.bit(idx) != passable)
            {
                Error{standard_error()} << "!!! fatal: cell" << i << j << "bit" << (int)g.bit(idx)
                                        << "but every position says passable ==" << (int)passable;
                fm_assert(false);
            }
        }
}

// pack_bit_index_from_coord() adds half_tile then floors, so cell i holds exactly the integer
// positions [i*div_size - half_tile, (i+1)*div_size - half_tile - 1]. build_impl() inverts this.
void test_cell_spans_match_forward_map(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());

    const auto dc = g.div_count();
    const int d = (int)div_size;
    constexpr int half_tile = tile_size_xy/2;

    for (uint8_t ly = 0; ly < 3; ly++)
        for (uint8_t lx = 0; lx < 3; lx++)
            for (int oy = -half_tile; oy < half_tile; oy++)
                for (int ox = -half_tile; ox < half_tile; ox++)
                {
                    const int px = lx*tile_size_xy + ox, py = ly*tile_size_xy + oy;
                    const int i = (px + half_tile) / d, j = (py + half_tile) / d;
                    const auto idx = g.get_bitmask_index_from_coord(local_coords{lx, ly},
                                                                    Vector2b{(int8_t)ox, (int8_t)oy});
                    fm_assert(idx == Pass::Grid::get_bitmask_index((uint32_t)i, (uint32_t)j, dc));
                    fm_assert(px >= i*d - half_tile && px < (i+1)*d - half_tile);
                    fm_assert(py >= j*d - half_tile && py < (j+1)*d - half_tile);
                }
}

uint32_t cell_x_of(const Pass::Grid& g, int px)
{
    constexpr int half_tile = tile_size_xy/2;
    const int lx = (px + half_tile) / tile_size_xy, ox = px - lx*tile_size_xy;
    fm_assert(lx >= 0 && lx < (int)TILE_MAX_DIM && ox >= -half_tile && ox < half_tile);
    auto idx = g.get_bitmask_index_from_coord(local_coords{(uint8_t)lx, 0}, Vector2b{(int8_t)ox, 0});
    return idx % g.div_count();
}

// build_impl() anchors cell i at a = i*div_size + div_size/2 - half_tile, then inflates obstacles
// by the cell's reach around a. div_size/2 rounds down, so a is not the cell's midpoint: the reach
// is div_size/2 below and div_size-1-div_size/2 above.
void test_div_anchor_matches_cell_span(uint32_t div_size, uint32_t bbox_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size, bbox_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());
    fm_assert(pool.params().div_size == div_size);
    fm_assert(pool.params().bbox_size == bbox_size);

    const int dc = (int)g.div_count(), d = (int)div_size;
    constexpr int half_tile = tile_size_xy/2;
    const int below = d/2, above = d - 1 - d/2;
    const auto hb = (float)bbox_size * .5f;

    for (int i = 0; i < dc; i++)
    {
        const int a = i*d + d/2 - half_tile;
        const auto r = g.get_coord_from_div((uint32_t)i, (uint32_t)i);
        fm_assert(r.min().x() == (float)a - hb && r.max().x() == (float)a + hb);
        fm_assert(r.min().y() == (float)a - hb && r.max().y() == (float)a + hb);
        fm_assert(cell_x_of(g, a) == (uint32_t)i);
        fm_assert(cell_x_of(g, a - below) == (uint32_t)i);
        fm_assert(cell_x_of(g, a + above) == (uint32_t)i);
        if (i > 0)
            fm_assert(cell_x_of(g, a - below - 1) == (uint32_t)i - 1);
        if (i + 1 < dc)
            fm_assert(cell_x_of(g, a + above + 1) == (uint32_t)i + 1);
    }
}

struct exact_config
{
    uint32_t div_size, bbox_size;
    Vector2b chunk_delta;
    uint8_t at_x, at_y;
    Vector2b obstacle_offset;
    uint8_t obstacle_size;
};

constexpr exact_config exact_configs[] = {
    {  2,  4, { 0,  0},  4,  4, {  2,   2},  2 },  // div_size/2 rounds down to 1, so the reach differs per side
    {  1,  4, { 0,  0},  3,  5, {  0,   1},  4 },  // one position per cell, no reach either way
    {  4,  4, { 0,  0},  7,  2, { -3,   5},  6 },
    {  4,  8, { 0,  0},  2,  9, {  5,  -7},  2 },
    {  8,  8, { 0,  0},  9,  3, { -1,   6},  8 },
    {  2,  5, { 0,  0},  6,  6, {  3,  -2},  4 },  // odd bbox_size, so half_bbox lands on .5
    { 16, 16, { 0,  0},  5,  8, { 11,  -9}, 10 },
    {  4,  8, { 1,  0},  0,  6, {-32,   3},  8 },  // reaches back from the east neighbor
    {  2,  4, { 0, -1},  7, 15, {  5,  31},  8 },  // reaches back from the north neighbor
};

// Brute-force the bit against Search::is_passable_ at every position the cell holds — the same
// query cache::is_passable_for_bbox() falls back to when the chunk has no grid.
void test_bit_exact(const exact_config& cfg)
{
    const chunk_coords_ obstacle_ch{(int16_t)cfg.chunk_delta.x(), (int16_t)cfg.chunk_delta.y(), 0};

    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    auto& oc = w[obstacle_ch];
    add_ground_all(oc);
    {
        scenery_proto p;
        p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
        p.subtype   = generic_scenery_proto{};
        p.offset    = cfg.obstacle_offset;
        p.bbox_size = Vector2ub(cfg.obstacle_size);
        p.pass      = pass_mode::blocked;
        w.make_scenery(w.make_id(), {obstacle_ch, local_coords{cfg.at_x, cfg.at_y}}, move(p));
    }
    rebuild_passability(c);
    rebuild_passability(oc);

    Pass::Pool pool{Pass::Params{cfg.div_size, cfg.bbox_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());
    fm_assert(pool.params().div_size == cfg.div_size);
    fm_assert(pool.params().bbox_size == cfg.bbox_size);

    const auto nbs = w.neighbors(COORD);
    const auto& pred = Search::without_critters();
    const int dc = (int)g.div_count(), d = (int)cfg.div_size;
    constexpr int half_tile = tile_size_xy/2;
    const auto hb = (float)cfg.bbox_size * .5f;

    // scenery_tile(): center - bbox_size/2, then + bbox_size
    const int mx = cfg.chunk_delta.x()*chunk_size_xy + cfg.at_x*tile_size_xy
                   + cfg.obstacle_offset.x() - cfg.obstacle_size/2,
              my = cfg.chunk_delta.y()*chunk_size_xy + cfg.at_y*tile_size_xy
                   + cfg.obstacle_offset.y() - cfg.obstacle_size/2;
    const int Mx = mx + cfg.obstacle_size, My = my + cfg.obstacle_size;
    // only the cells that can reach the obstacle; a full dc² sweep is 1024² at div_size 1
    const int pad = (int)cfg.bbox_size + 2*d;
    const int i_lo = Math::max(0, (mx - pad + half_tile) / d), i_hi = Math::min(dc - 1, (Mx + pad + half_tile) / d),
              j_lo = Math::max(0, (my - pad + half_tile) / d), j_hi = Math::min(dc - 1, (My + pad + half_tile) / d);
    fm_assert(i_lo <= i_hi && j_lo <= j_hi);

    uint32_t cleared = 0;
    for (int j = j_lo; j <= j_hi; j++)
        for (int i = i_lo; i <= i_hi; i++)
        {
            bool passable = true;
            for (int py = j*d - half_tile; py < (j+1)*d - half_tile && passable; py++)
                for (int px = i*d - half_tile; px < (i+1)*d - half_tile && passable; px++)
                    passable = Search::is_passable_(&c, nbs, Vector2{(float)px - hb, (float)py - hb},
                                                             Vector2{(float)px + hb, (float)py + hb}, pred);
            cleared += !passable;
            const auto idx = Pass::Grid::get_bitmask_index((uint32_t)i, (uint32_t)j, (uint32_t)dc);
            if (g.bit(idx) != passable)
            {
                Error{standard_error()} << "!!! fatal: div_size" << d << "bbox_size" << (int)cfg.bbox_size
                                        << "cell" << i << j << "bit" << (int)g.bit(idx)
                                        << "but every position says passable ==" << (int)passable;
                fm_assert(false);
            }
        }
    // nothing outside the window may be cleared, and the config has to clear something
    fm_assert(cleared > 0);
    fm_assert((uint32_t)(dc*dc) - count_passable(g) == cleared);
}

// A hole marker is a collision_type::none rtree entry, never a collider. Its pass_mode only
// picks which pass_through_mask filter_bbox_through_holes() cuts for. search.cpp, raycast.cpp
// and sweep-aabb.cpp all skip type none no matter the mode; build_impl() must agree.
void test_hole_marker_never_blocks(uint32_t div_size, pass_mode hole_pass)
{
    auto w = world();
    auto& c = w[COORD];
    add_ground_all(c);
    auto h = w.make_object<hole>(w.make_id(), global_coords{COORD, {8, 8}}, hole_proto{});
    h->set_bbox({}, {}, Vector2ub{48, 32}, hole_pass);
    rebuild_passability(c);

    // "tiles" ground is pass_mode::pass, so the marker is the only entry
    fm_assert(c.rtree()->Count() == 1);
    constexpr auto ctr = Vector2(tile_size_xy*8);
    fm_assert(Search::is_passable_1(c, ctr - Vector2{8}, ctr + Vector2{8}, Search::without_critters()));

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());
    const auto dc = g.div_count();
    if (count_passable(g) != dc*dc)
    {
        Error{standard_error()} << "!!! fatal: div_size" << div_size << "hole pass mode"
                                << (int)hole_pass << "cleared"
                                << dc*dc - count_passable(g) << "of" << dc*dc << "cells";
        fm_assert(false);
    }
}

// The cut happens at rtree build time, so skipping the marker must not close the opening.
void test_hole_still_opens_blocked_ground(uint32_t div_size)
{
    auto w = world();
    auto& c = w[COORD];
    add_blocked_ground_all(c);
    auto h = w.make_object<hole>(w.make_id(), global_coords{COORD, {8, 8}}, hole_proto{});
    h->set_bbox({}, {}, Vector2ub{tile_size_xy}, pass_mode::pass);
    rebuild_passability(c);

    Pass::Pool pool{Pass::Params{div_size}};
    tick(w, pool);
    Pass::Grid g = pool[c];
    g.build_if_stale(Search::without_critters());
    const auto dc = g.div_count(), n = count_passable(g);
    fm_assert(n > 0 && n < dc*dc);
}

} // namespace

void test_grid()
{
    for (const auto ds : { 16u, 8u })
    {
        test_ground_only_is_fully_passable(ds);
        test_wall_blocks_some_cells(ds);
        test_idempotent_rebuild(ds);
        test_neighbor_cascade(ds);
        test_collect_cleanup(ds);
        test_add_remove_wall_restores_baseline(ds);
        test_neighbor_west_wall_affects_east_edge(ds);
        test_bit_from_tile_center_passable(ds);
        test_wall_then_clear_ground_allows_collect(ds);
        test_cell_at_wall_is_blocked(ds);
        test_cell_south_of_wall_is_passable(ds);
        test_all_chunk_corners_passable(ds);
        test_div_count_derived_from_div_size(ds);
        test_blocked_ground_blocks_all(ds);
        test_multi_chunk_3x3_sanity(ds);
        test_chunk_reinsertion_after_collect(ds);
        test_frame_counter_ticks_independently(ds);
        test_empty_pool_safe_tick(ds);
        test_last_bit_index_valid(ds);
        test_wall_west_blocks(ds);
        test_many_walls_strictly_reduce_passable(ds);
        test_repeated_ticks_noop_on_stable_world(ds);
        test_explicit_wrapper_mark_stale_rebuilds(ds);
        test_mark_modified_only_without_ensure_passability(ds);
        test_neighbor_mark_only_without_ensure_detected(ds);
        test_params_preserved_through_pool(ds);
        test_collect_all_chunks_no_crash(ds);
        test_pool_reuse_after_collect(ds);
        test_pool_reuse_neighbors_collected(ds);
        test_neighbor_lifecycle(ds);
        test_self_collect_with_neighbor(ds);
        test_pool_reuse_across_different_coord(ds);
        test_mark_and_ensure_preserve_frame_no(ds);
        test_multiple_pooled_items(ds);
        test_bitview_read_matches_bit(ds);
        test_partial_collect_then_collect_survivors(ds);
    }
    test_pool_destruction_with_live_grids();
    test_chunk_pass_gen_unique_after_collect();
    test_chunk_pass_gen_monotonic_after_move_ctor();
    test_chunk_pass_gen_monotonic_after_move_assign();
    for (const auto ds : { 1u, 2u, 4u, 16u, 64u })
    {
        test_cell_spans_match_forward_map(ds);
        test_div_anchor_matches_cell_span(ds, Math::max(4u, ds));
    }
    for (const auto& cfg : exact_configs)
        test_bit_exact(cfg);
    for (const auto ds : { 4u, 16u })
    {
        for (const auto p : { pass_mode::blocked, pass_mode::see_through,
                              pass_mode::shoot_through, pass_mode::pass })
            test_hole_marker_never_blocks(ds, p);
        test_hole_still_opens_blocked_ground(ds);
    }
    test_bit_matches_every_position();
}

} // namespace floormat::Test
