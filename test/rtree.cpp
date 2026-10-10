#include "app.hpp"
#include "src/world.hpp"
#include "src/critter.hpp"
#include "src/scenery-proto.hpp"
#include "src/light.hpp"
#include "src/hole.hpp"
#include "src/search.hpp"
#include "src/point.inl"
#include "src/RTree.hpp"
#include "src/RTree-search.hpp"
#include "src/nanosecond.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"

namespace floormat {

namespace {

critter_proto make_critter_proto()
{
    critter_proto proto;
    proto.atlas = loader.anim_atlas("npc-walk", loader.ANIM_PATH);
    proto.name = "critter"_s;
    proto.speed = 1;
    proto.playable = true;
    proto.offset = {};
    proto.bbox_offset = {};
    proto.bbox_size = Vector2ub(tile_size_xy/2);
    return proto;
}

void test1()
{
    auto w = world();
    constexpr auto ch = chunk_coords_{0, 0, 0};
    constexpr auto ch2 = chunk_coords_{2, 2, 0};
    constexpr auto pos = global_coords{ch, {0, 0}};
    constexpr auto pos2 = global_coords{ch2, {0, 0}};
    auto& c = w[ch];
    auto& c2 = w[ch2];

    fm_assert(c.rtree()->Count() == 0);
    auto C = w.make_object<critter>(w.make_id(), pos, make_critter_proto());
    fm_assert(C->offset == Vector2b{});
    fm_assert(c.objects().size() == 1);
    fm_assert(c.rtree()->Count() == 1);
    fm_assert(c2.rtree()->Count() == 0);
    auto index = C->index();
    C->teleport_to(index, pos, {1, 2}, rotation::N);
    fm_assert(C->offset == Vector2b{1, 2});
    fm_assert(c.rtree()->Count() == 1);
    fm_assert(c2.rtree()->Count() == 0);
    C->teleport_to(index, pos2, {}, rotation::N);
    fm_assert(c.objects().size() == 0);
    fm_assert(c.rtree()->Count() == 0);
    fm_assert(c2.rtree()->Count() == 1);
    (void)index;
}

void test2()
{
    auto w = world();
    constexpr auto ch = chunk_coords_{0, 0, 0};
    constexpr auto pos = global_coords{ch, {0, 0}};
    constexpr auto ch2 = chunk_coords_{-1, 0, 0};
    auto& c = w[ch];
    auto& c2 = w[ch2];

    fm_assert(c.rtree()->Count() == 0);
    auto C = w.make_object<critter>(w.make_id(), pos, make_critter_proto());
    fm_assert(C->offset == Vector2b{});
    fm_assert(c.objects().size() == 1);
    fm_assert(c.rtree()->Count() == 1);
    auto index = C->index();
    bool moved = false;
    c.ensure_passability();
    for (int i = 0; i < 60*2; i++)
    {
        constexpr auto dt = Ns{Seconds / 60};
        auto dtʹ = dt;
        auto result = C->move_toward(index, dtʹ, {{-10, 0, 0}, {}, {}});
        fm_assert(!result.blocked);
        moved |= result.moved;
        fm_assert(!c.is_passability_modified());

        auto cnt = c.rtree()->Count(), cnt2 = c2.rtree()->Count();
        fm_assert(cnt <= 1);
        fm_assert(cnt2 <= 1);
        fm_assert_not_equal(cnt, cnt2);
    }
    fm_assert(moved);
    fm_assert_equal(ch2, C->coord.chunk3());
    fm_assert_equal(0, c.rtree()->Count());
    fm_assert_equal(1, c2.rtree()->Count());
}

void test3()
{
    auto w = world();
    constexpr auto ch = chunk_coords_{0, 0, 0};
    constexpr auto pos = global_coords{ch, {8, 8}};

    auto proto = make_critter_proto();
    proto.r = rotation::N;
    proto.bbox_size = {10, 58};
    auto C = w.make_object<critter>(w.make_id(), pos, move(proto));

    // the box turned east only touches this one in place, so can_rotate passes
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_offset = {36, 0};
    p.bbox_size   = {14, 6};
    p.pass        = pass_mode::blocked;
    w.make_scenery(w.make_id(), pos, move(p));

    fm_assert(C->can_move_to({}));
    auto index = C->index();
    auto dt = Ns{Seconds};
    const auto result = C->move_toward(index, dt, point{pos, {11, 0}});
    fm_assert(C->can_move_to({}));
    fm_assert(result.blocked && !result.moved);
}

uint32_t entries_at(const chunk::RTree& t, Vector2 pt)
{
    uint32_t n = 0;
    const auto pt2 = pt + Vector2{1e-4f};
    t.Search(pt.data(), pt2.data(), [&](object_id, const chunk::RTree::Rect&) { n++; return true; });
    return n;
}

// A light change on a dirty chunk waits for ensure_passability(), like a dynamic object's.
// No light change dirties passability.
void test_light_tree()
{
    auto w = world();
    constexpr auto ch = chunk_coords_{0, 0, 0};
    constexpr auto ch2 = chunk_coords_{1, 0, 0};
    auto& c = w[ch];
    auto& c2 = w[ch2];

    fm_assert(!c2.light_rtree());
    c2.mark_passability_modified();

    fm_assert(c.is_passability_modified());
    auto L = w.make_object<light>(w.make_id(), global_coords{ch, {2, 2}}, light_proto{});
    fm_assert(c.is_passability_modified());
    fm_assert(c.light_rtree() && c.light_rtree()->Count() == 1);
    fm_assert(c.rtree()->Count() == 0);
    fm_assert(c.hole_rtree()->Count() == 0);

    auto index = L->index();
    L->teleport_to(index, global_coords{ch, {5, 5}}, {3, 4}, L->r);
    fm_assert(!c.is_passability_modified());
    const auto ctr = Vector2{5, 5} * tile_size<Vector2> + Vector2{3, 4};
    fm_assert(entries_at(*c.light_rtree(), Vector2{2, 2} * tile_size<Vector2>) == 0);
    fm_assert(entries_at(*c.light_rtree(), ctr + Vector2{20, 0}) == 1);

    L->set_bbox(L->offset, {}, Vector2ub{32, 16}, pass_mode::blocked);
    fm_assert(!c.is_passability_modified());
    fm_assert(c.light_rtree()->Count() == 1);
    fm_assert(entries_at(*c.light_rtree(), ctr + Vector2{12, 0}) == 1);
    fm_assert(entries_at(*c.light_rtree(), ctr + Vector2{20, 0}) == 0);
    fm_assert(c.rtree()->Count() == 0);

    fm_assert(c2.is_passability_modified());
    L->teleport_to(index, global_coords{ch2, {0, 0}}, {}, L->r);
    fm_assert(!c.is_passability_modified());
    fm_assert(c.light_rtree()->Count() == 0);
    fm_assert(c2.is_passability_modified());
    fm_assert(c2.light_rtree() && c2.light_rtree()->Count() == 1);
    fm_assert(c2.rtree()->Count() == 0);

    c2.mark_passability_modified();
    c2.remove_object(*L, L->index());
    fm_assert(c2.light_rtree()->Count() == 0);
}

// filter_bbox_through_holes() reads the hole tree and writes the cut pieces into the collision
// tree. The hole sits on the east edge, so the neighbor's hole tree holds it as well.
void test_hole_tree()
{
    auto w = world();
    constexpr auto ch = chunk_coords_{0, 0, 0};
    constexpr auto ch2 = chunk_coords_{1, 0, 0};
    constexpr auto pos = global_coords{ch, {15, 8}};
    auto& c = w[ch];
    auto& c2 = w[ch2];

    scenery_proto p;
    p.atlas   = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype = generic_scenery_proto{};
    p.pass    = pass_mode::blocked;
    w.make_scenery(w.make_id(), pos, move(p));

    hole_proto hp;
    hp.bbox_size = {22, 18};
    (void)w.make_object<hole>(w.make_id(), pos, hp);

    fm_assert(c.hole_rtree()->Count() == 1);
    fm_assert(c2.hole_rtree()->Count() == 1);
    fm_assert(c.rtree()->Count() > 1);

    const auto ctr = Vector2{15, 8} * tile_size<Vector2>;
    const auto solid = ctr + Vector2{24, 0};
    fm_assert(Search::is_passable_1(c, ctr - Vector2{4}, ctr + Vector2{4}, Search::without_critters()));
    fm_assert(!Search::is_passable_1(c, solid - Vector2{4}, solid + Vector2{4}, Search::without_critters()));

    uint32_t found = 0;
    chunk::get_all_holes_in_bbox([&](Math::Range2D<float>, Math::Range1D<uint8_t>) { found++; },
                                 c, ctr - Vector2{40}, ctr + Vector2{40}, can_see_through_mask);
    fm_assert(found == 1);
}

} // namespace


void Test::test_rtree()
{
    test1();
    test2();
    test3();
    test_light_tree();
    test_hole_tree();
}

} // namespace floormat
