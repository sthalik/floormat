#include "app.hpp"
#include "src/world.hpp"
#include "src/critter.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/light.hpp"
#include "src/hole.hpp"
#include "src/collision.hpp"
#include "src/tile-image.hpp"
#include "src/wall-atlas.hpp"
#include "src/search.hpp"
#include "src/point.inl"
#include "src/RTree.hpp"
#include "src/sweep-aabb.hpp"
#include "src/RTree-search.hpp"
#include "src/nanosecond.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include <bit>
#include <cr/Optional.h>

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

void test_pass_phases()
{
    auto w = world();
    constexpr auto pos = global_coords{chunk_coords_{0, 0, 0}, {8, 8}};

    scenery_proto p;
    p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype   = generic_scenery_proto{};
    p.bbox_size = {20, 20};
    p.pass      = pass_mode::blocked;
    w.make_scenery(w.make_id(), pos, move(p));

    scenery_proto p2;
    p2.atlas     = loader.invalid_scenery_atlas().proto->atlas;
    p2.subtype   = generic_scenery_proto{};
    p2.bbox_size = {10, 30};
    p2.pass      = pass_mode::pass;
    auto s = w.make_scenery(w.make_id(), pos, move(p2));

    fm_assert(s->can_move_to({}));
    fm_assert(s->can_rotate(rotation::E));
}

// A 63 px box is the 64 px one without its leftmost pixel.
void test_odd_bbox_size()
{
    auto w = world();
    constexpr auto ch = chunk_coords_{0, 0, 0};
    auto& c = w[ch];
    const auto proto = [](Vector2b offset, Vector2ub size) {
        scenery_proto p;
        p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
        p.subtype   = generic_scenery_proto{};
        p.offset    = offset;
        p.bbox_size = size;
        p.pass      = pass_mode::blocked;
        return p;
    };

    auto s = w.make_scenery(w.make_id(), {ch, {8, 8}}, proto({}, Vector2ub{63}));
    c.ensure_passability();
    uint32_t n = 0;
    const auto lo = Vector2{0}, hi = Vector2{(float)chunk_size_xy};
    c.rtree()->Search(lo.data(), hi.data(), [&](object_id, const chunk::RTree::Rect& r) {
        fm_assert(Vector2(r.m_min[0], r.m_min[1]) == Vector2{8*64 - 31});
        fm_assert(Vector2(r.m_max[0], r.m_max[1]) == Vector2{8*64 + 32});
        n++;
        return true;
    });
    fm_assert(n == 1);
    fm_assert(s->can_move_to({}));

    for (const auto size : { Vector2ub{63}, Vector2ub{64} })
    {
        fm_assert(c.can_place_object(proto({1, 0}, size), {7, 8}));
        fm_assert(!c.can_place_object(proto({2, 0}, size), {7, 8}));
    }
    fm_assert(c.can_place_object(proto({-1, 0}, Vector2ub{63}), {9, 8}));
    fm_assert(!c.can_place_object(proto({-2, 0}, Vector2ub{63}), {9, 8}));
    fm_assert(c.can_place_object(proto({0, 0}, Vector2ub{64}), {9, 8}));
    fm_assert(!c.can_place_object(proto({-1, 0}, Vector2ub{64}), {9, 8}));
}

void test_corner_touch()
{
    const auto start = Range2D{{0, 0}, {10, 10}};
    constexpr auto V = Vector2{20, 20};
    fm_assert(!sweep_aabb_vs_aabb(start, V, Range2D{{20, -10}, {30, 10}}).has_collider);
    fm_assert(sweep_aabb_vs_aabb(start, V, Range2D{{20, -9}, {30, 11}}).has_collider);
}

void test_bbox_offset_min()
{
    auto w = world();
    constexpr auto pos = global_coords{chunk_coords_{0, 0, 0}, {8, 8}};
    constexpr auto bb = Vector2b{-127, -127};

    auto proto = make_critter_proto();
    proto.bbox_offset = {-128, -128};
    auto C = w.make_object<critter>(w.make_id(), pos, move(proto));
    fm_assert(C->bbox_offset == bb);

    auto index = C->index();
    C->teleport_to(index, pos, {1, 0}, rotation_COUNT);
    fm_assert(C->bbox_offset == bb);
    for (auto r : {rotation::E, rotation::S, rotation::W, rotation::N})
        C->rotate(index, r);
    fm_assert(C->bbox_offset == bb);

    C->set_bbox(C->offset, {-128, 0}, C->bbox_size, C->pass);
    fm_assert(C->bbox_offset == Vector2b(-127, 0));
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

Optional<collision_data> entry_at(chunk& c, Vector2 pt)
{
    Optional<collision_data> ret;
    const auto pt2 = pt + Vector2{1e-4f};
    c.rtree()->Search(pt.data(), pt2.data(), [&](object_id id, const chunk::RTree::Rect&) {
        fm_assert(!ret);
        ret = std::bit_cast<collision_data>(id);
        return true;
    });
    return ret;
}

bool passable_at(chunk& c, Vector2 pt)
{
    return Search::is_passable_1(c, pt - Vector2{2}, pt + Vector2{2}, Search::without_critters());
}

// The part of a wall, scenery or ground under a more open hole takes the hole's pass mode, so a
// window stays solid to walkers. A hole of equal mode cuts scenery too.
void test_hole_pass_modes()
{
    constexpr auto ch = chunk_coords_{0, 0, 0};
    constexpr auto pos = global_coords{ch, {8, 8}};
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    const auto ctr = Vector2{8, 8} * tile_size<Vector2>;
    const auto mid = Vector2{ctr.x(), ctr.y() - half_tile<float> - (float)W.atlas->info().depth/2};
    const auto side = mid - Vector2{24, 0};

    hole_proto hp;
    hp.offset = {0, -tile_size_xy/2};
    hp.bbox_size = {32, 32};

    for (auto p : { pass_mode::pass, pass_mode::see_through, pass_mode::shoot_through })
    {
        auto w = world();
        auto& c = w[ch];
        c[{8, 8}].wall_north() = W;
        hp.pass = p;
        (void)w.make_object<hole>(w.make_id(), pos, hp);

        const auto e = entry_at(c, mid);
        if (p == pass_mode::pass)
            fm_assert(!e);
        else
            fm_assert(e && e->type == (uint64_t)collision_type::geometry && e->pass == (uint64_t)p);
        fm_assert(passable_at(c, mid) == (p == pass_mode::pass));
        const auto s = entry_at(c, side);
        fm_assert(s && s->pass == (uint64_t)pass_mode::blocked);
    }

    {
        auto w = world();
        auto& c = w[ch];
        c[{8, 8}].wall_north() = W;
        hp.pass = pass_mode::see_through;
        (void)w.make_object<hole>(w.make_id(), pos, hp);
        auto hp2 = hp;
        hp2.offset.x() = 16;
        hp2.pass = pass_mode::pass;
        (void)w.make_object<hole>(w.make_id(), pos, hp2);

        const auto e = entry_at(c, mid - Vector2{8, 0});
        fm_assert(e && e->pass == (uint64_t)pass_mode::see_through);
        fm_assert(!entry_at(c, mid + Vector2{8, 0}));
        fm_assert(!entry_at(c, mid + Vector2{24, 0}));
        const auto s = entry_at(c, side);
        fm_assert(s && s->pass == (uint64_t)pass_mode::blocked);
    }

    {
        auto w = world();
        auto& c = w[ch];
        scenery_proto p;
        p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
        p.subtype     = generic_scenery_proto{};
        p.bbox_offset = {};
        p.bbox_size   = {32, 32};
        p.pass        = pass_mode::blocked;
        w.make_scenery(w.make_id(), pos, move(p));
        hole_proto hp3;
        hp3.bbox_size = {16, 16};
        hp3.pass = pass_mode::shoot_through;
        (void)w.make_object<hole>(w.make_id(), pos, hp3);

        const auto e = entry_at(c, ctr);
        fm_assert(e && e->type == (uint64_t)collision_type::scenery && e->pass == (uint64_t)pass_mode::shoot_through);
        fm_assert(!passable_at(c, ctr));
        const auto s = entry_at(c, ctr - Vector2{12, 0});
        fm_assert(s && s->pass == (uint64_t)pass_mode::blocked);
    }

    for (auto p : { pass_mode::see_through, pass_mode::pass })
    {
        auto w = world();
        auto& c = w[ch];
        scenery_proto sp;
        sp.atlas       = loader.invalid_scenery_atlas().proto->atlas;
        sp.subtype     = generic_scenery_proto{};
        sp.bbox_offset = {};
        sp.bbox_size   = {32, 32};
        sp.pass        = p;
        w.make_scenery(w.make_id(), pos, move(sp));
        hole_proto hp5;
        hp5.bbox_size = {16, 16};
        hp5.pass = p;
        (void)w.make_object<hole>(w.make_id(), pos, hp5);

        const auto e = entry_at(c, ctr);
        if (p == pass_mode::pass)
            fm_assert(!e);
        else
            fm_assert(e && e->type == (uint64_t)collision_type::scenery && e->pass == (uint64_t)p);
        const auto s = entry_at(c, ctr - Vector2{12, 0});
        fm_assert(s && s->type == (uint64_t)collision_type::scenery && s->pass == (uint64_t)p);
        uint32_t pieces = 0;
        const auto min = ctr - Vector2{32}, max = ctr + Vector2{32};
        c.rtree()->Search(min.data(), max.data(), [&](object_id, const chunk::RTree::Rect&) {
            pieces++;
            return true;
        });
        fm_assert(pieces > 1);
    }

    {
        auto w = world();
        auto& c = w[ch];
        c[{8, 8}].ground() = tile_image_proto{ loader.ground_atlas("texel"), 0 };
        hole_proto hp4;
        hp4.bbox_size = {16, 16};
        hp4.pass = pass_mode::see_through;
        (void)w.make_object<hole>(w.make_id(), pos, hp4);

        const auto e = entry_at(c, ctr);
        fm_assert(e && e->type == (uint64_t)collision_type::geometry && e->pass == (uint64_t)pass_mode::see_through);
        const auto s = entry_at(c, ctr - Vector2{12, 0});
        fm_assert(s && s->pass == (uint64_t)pass_mode::blocked);
    }
}

} // namespace


void Test::test_rtree()
{
    test1();
    test2();
    test3();
    test_pass_phases();
    test_odd_bbox_size();
    test_corner_touch();
    test_bbox_offset_min();
    test_light_tree();
    test_hole_tree();
    test_hole_pass_modes();
}

} // namespace floormat
