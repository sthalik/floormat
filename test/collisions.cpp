#include "app.hpp"
#include "src/world.hpp"
#include "src/critter.hpp"
#include "src/RTree.hpp"
#include "src/nanosecond.inl"
#include "loader/loader.hpp"

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

} // namespace


void Test::test_collisions()
{
    test1();
    test2();
}

} // namespace floormat
