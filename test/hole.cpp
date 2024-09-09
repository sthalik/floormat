#include "app.hpp"
#include "src/hole.hpp"
#include "src/hole-cut.hpp"
#include "src/tile-constants.hpp"

namespace floormat {
namespace {

using bbox = CutResult<Int>::bbox;

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
            if (res.array[i].min == min && res.array[i].max == max)
                return true;
        return false;
    };
}

void test2()
{
    const auto res = CutResult<int>::cut({{}, Vector2ub{tile_size_xy}}, {Vector2i(-tile_size_xy/2), Vector2ub{tile_size_xy}});
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

} // namespace

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
}

} // namespace floormat
