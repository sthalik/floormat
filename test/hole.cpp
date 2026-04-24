#include "app.hpp"
#include "run.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include "src/nanosecond.inl"
#include "src/tile-image.hpp"
#include "src/hole.hpp"
#include "src/hole-cut.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"

namespace floormat {
namespace {

struct bbox
{
    Vector2i position;
    Vector2ub bbox_size;

    constexpr operator Math::Range2D<Int>() const
    {
        auto min = position - Vector2i{bbox_size/2};
        return { min, min + Vector2i{bbox_size} };
    }
};

auto cut(bbox rect, bbox hole, Vector2i offset)
{
    auto rectʹ = bbox { rect.position + offset, rect.bbox_size };
    auto holeʹ = bbox { hole.position + offset, hole.bbox_size };
    return CutResult<Int>::cut(rectʹ, holeʹ).size;
}

void test1(Vector2i offset)
{
    constexpr auto rect = bbox{{}, {50, 50}};
#if 1
    fm_assert_not_equal(0, cut(rect, {{ 49,   0}, {50, 50}}, offset));
    fm_assert_not_equal(0, cut(rect, {{  0,  49}, {50, 50}}, offset));
    fm_assert_not_equal(0, cut(rect, {{ 49,  49}, {50, 50}}, offset));
#endif
#if 1
    fm_assert_not_equal(0, cut(rect, {{-49,   0}, {50, 50}}, offset));
    fm_assert_not_equal(0, cut(rect, {{  0, -49}, {50, 50}}, offset));
    fm_assert_not_equal(0, cut(rect, {{ 49, -49}, {50, 50}}, offset));
#endif
#if 1
    fm_assert_equal(0, cut(rect, {{ 0,  0}, {50, 50}}, offset));
    fm_assert_equal(1, cut(rect, {{ 0,  0}, {49, 50}}, offset));
    fm_assert_equal(1, cut(rect, {{ 1,  0}, {50, 50}}, offset));
    fm_assert_equal(1, cut(rect, {{50,  0}, {50, 50}}, offset));
    fm_assert_equal(1, cut(rect, {{ 0, 50}, {50, 50}}, offset));
    fm_assert_equal(1, cut(rect, {{50, 50}, {50, 50}}, offset));
#endif
#if 1
    fm_assert_equal(0, cut(rect, {{ 9,  9}, {70, 70}}, offset));
    fm_assert_equal(2, cut(rect, {{11, 11}, {70, 70}}, offset));
    fm_assert_equal(1, cut(rect, {{10, 11}, {70, 70}}, offset));
    fm_assert_equal(0, cut(rect, {{10, 10}, {70, 70}}, offset));
    fm_assert_equal(2, cut(rect, {{20, 20}, {70, 70}}, offset));
#endif
#if 1
    fm_assert_equal(1, cut(rect, {{ 1,  0}, {50, 50}}, offset));
    fm_assert_equal(1, cut(rect, {{ 0,  1}, {50, 50}}, offset));
    fm_assert_equal(2, cut(rect, {{ 1,  1}, {50, 50}}, offset));
    fm_assert_equal(2, cut(rect, {{49, 49}, {50, 50}}, offset));
    fm_assert_equal(1, cut(rect, {{50, 50}, {50, 50}}, offset));
#endif
#if 1
    // todo! coverage
#endif
}

auto make_search_predicate(const CutResult<int>& res)
{
    return [&](Vector2i min, Vector2i max) -> bool {
        for (auto i = 0u; i < res.size; i++)
            if (res.array[i].min() == min && res.array[i].max() == max)
                return true;
        return false;
    };
}

void test2()
{
    const auto res = CutResult<int>::cut(bbox{{}, Vector2ub{tile_size_xy}}, bbox{Vector2i(-tile_size_xy/2), Vector2ub{tile_size_xy}});
    fm_assert(res.size == 2);
    const auto has = make_search_predicate(res);
    fm_assert(has({-32, 0}, {32, 32}));
    fm_assert(has({0, -32}, {32, 0}));
}

void test3()
{
    constexpr auto h = tile_size_xy/2;

    {
        const auto res = CutResult<Int>::cut({-h, -1}, {h, 1}, {-2, -100}, {2, 100});
        fm_assert(res.found());
        fm_assert_equal(2, (int)res.size);
    }
    {
        const auto res = CutResult<Int>::cut({-h, 0}, {h, 0}, {-2, -100}, {2, 100});
        fm_assert(res.found());
        fm_assert_equal(2, (int)res.size);
        const auto has = make_search_predicate(res);
        fm_assert(has({-h, 0}, {-2, 0}));
        fm_assert(has({ 2, 0}, { h, 0}));
    }
}

void test_degenerate()
{
    constexpr auto h = tile_size_xy*.5f;

    struct WallPos {
        Vector2 left, right;
    };

    constexpr auto w = WallPos {
        { -h, -h },
        {  h, -h },
    };

    {
        // degenerate case with no area
        const auto res = CutResult<float>::cut(w.left, w.right, w.left - Vector2{0, 1}, {});
        fm_assert(res.found());
        fm_assert_equal(1, res.size);
        const auto x = res.array[0];
        constexpr auto expected = Range2D { { 0, -h}, {h, -h} };
        fm_assert_equal(expected.min(), x.min());
        fm_assert_equal(expected.max(), x.max());
    }
    {
        // now hole has no area
        const auto res = CutResult<float>::cut(w.left, w.right, {-h, -h}, {h, -h});
        fm_assert(!res.found());
        fm_assert_equal(1, res.size);
        fm_assert_equal(w.left, res.array[0].min());
        fm_assert_equal(w.right, res.array[0].max());
    }
}

} // namespace
} // namespace floormat

namespace floormat::Run {
namespace {

void test_walking1(StringView instance_name, Function make_dt, double accel, uint32_t max_steps)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    {
        auto w = world();
        w[{{0,0,0}, {8,8}}].t.wall_west() = W;

        bool ret1 = run(w, make_dt,
                       Start{
                           .name = "test_walking1"_s,
                           .instance = instance_name,
                           .pt = {{0,0,0}, {0,8}, {}},
                           .accel = accel,
                           .rotation = E,
                       },
                       Expected{
                           .pt = {{0,0,0}, {7, 8}, {8,0}}, // distance_L2 == 3
                           .time = 7600.15*Millisecond,
                       },
                       Grace{
                           .time = 300*Millisecond,
                           .distance_L2 = 4,
                           .max_steps = max_steps,
                       });
        fm_assert(ret1);
    }
    {
        auto w = world();
        w[{{0,0,0}, { 8,8}}].t.wall_west() = W;
        w[{{0,0,0}, {12,8}}].t.wall_west() = W;

        const auto p = hole_proto{};
        auto h = w.make_object<hole>(w.make_id(), global_coords{{0, 0, 0}, {8, 8}}, p);
        h->set_bbox({}, {}, {128, 48}, pass_mode::pass);

        bool ret2 = run(w, make_dt,
                       Start{
                           .name = "test_walking1_2"_s,
                           .instance = instance_name,
                           .pt = {{0,0,0}, {0,8}, {}},
                           .accel = accel,
                           .rotation = E,
                       },
                       Expected{
                           .pt = {{0,0,0}, {11, 8}, {8,0}}, // distance_L2 == 3
                           .time = 11.9*Seconds,
                       },
                       Grace{
                           .time = 300*Millisecond,
                           .distance_L2 = 4,
                           .max_steps = max_steps,
                       });
        fm_assert(ret2);
    }
}

} // namespace
} // namespace floormat::Run

namespace floormat {

void Test::test_hole()
{
    constexpr Vector2i offsets[] = {
        {  0,     0},
        { 110,  105},
        {  15,  110},
        {- 15, -110},
        {-110,  -15},
    };

    for (auto offset : offsets)
        test1(offset);

    test2();
    test3();
    test_degenerate();

    using namespace Run;

    test_walking1("dt=16.667 accel=1",   constantly(Millisecond * 16.667),    1, Grace::default_max_steps);
    test_walking1("dt=16.667 accel=2",   constantly(Millisecond * 16.667),    2, Grace::default_max_steps);
    test_walking1("dt=16.667 accel=5",   constantly(Millisecond * 16.667),    5, Grace::default_max_steps);
    test_walking1("dt=16.667 accel=0.5", constantly(Millisecond * 16.667),  0.5, Grace::default_max_steps*2);
    test_walking1("dt=33.334 accel=1",   constantly(Millisecond * 33.334),    1, Grace::default_max_steps);
    test_walking1("dt=33.334 accel=2",   constantly(Millisecond * 33.334),    2, Grace::default_max_steps);
    test_walking1("dt=33.334 accel=5",   constantly(Millisecond * 33.334),    5, Grace::default_max_steps);
    test_walking1("dt=33.334 accel=10",  constantly(Millisecond * 33.334),   10, Grace::default_max_steps);
    test_walking1("dt=50.000 accel=1",   constantly(Millisecond * 50.000),    1, Grace::default_max_steps);
    test_walking1("dt=50.000 accel=2",   constantly(Millisecond * 50.000),    2, Grace::default_max_steps);
    test_walking1("dt=50.000 accel=5",   constantly(Millisecond * 50.000),    5, Grace::default_max_steps);
    test_walking1("dt=100.00 accel=1",   constantly(Millisecond * 100.00),    1, Grace::default_max_steps);
    test_walking1("dt=100.00 accel=2",   constantly(Millisecond * 100.00),    2, Grace::default_max_steps);
    test_walking1("dt=100.00 accel=0.5", constantly(Millisecond * 100.00),  0.5, Grace::default_max_steps);
    test_walking1("dt=200.00 accel=1",   constantly(Millisecond * 200.00),    1, Grace::default_max_steps);
    test_walking1("dt=1.0000 accel=1",   constantly(Millisecond * 1.0000),    1, Grace::very_slow_max_steps);
    test_walking1("dt=1.0000 accel=0.5", constantly(Millisecond * 1.0000),  0.5, Grace::very_slow_max_steps);
}

} // namespace floormat
