#include "app.hpp"
#include "src/hole.hpp"

namespace floormat::Hole {
namespace {

using bbox = cut_rectangle_result::bbox;

template<Int x, Int y>
void test1()
{
    static constexpr auto vec = Vector2i{x, y};
    constexpr auto rect = bbox{{}, {50, 50}};
    constexpr auto cutʹ = [](bbox rect, bbox hole) {
        auto rectʹ = bbox { rect.position + vec, rect.bbox_size };
        auto holeʹ = bbox { hole.position + vec, hole.bbox_size };
        return cut_rectangle(rectʹ, holeʹ).size;
    };
#if 1
    fm_assert_not_equal(0, cutʹ(rect, {{ 49,   0}, {50, 50}}));
    fm_assert_not_equal(0, cutʹ(rect, {{  0,  49}, {50, 50}}));
    fm_assert_not_equal(0, cutʹ(rect, {{ 49,  49}, {50, 50}}));
#endif
#if 1
    fm_assert_not_equal(0, cutʹ(rect, {{-49,   0}, {50, 50}}));
    fm_assert_not_equal(0, cutʹ(rect, {{  0, -49}, {50, 50}}));
    fm_assert_not_equal(0, cutʹ(rect, {{ 49, -49}, {50, 50}}));
#endif
#if 1
    fm_assert_equal(0, cutʹ(rect, {{50,  0}, {50, 50}}));
    fm_assert_equal(0, cutʹ(rect, {{ 0, 50}, {50, 50}}));
    fm_assert_equal(0, cutʹ(rect, {{50, 50}, {50, 50}}));
#endif
#if 1
    fm_assert_equal(9, cutʹ(rect, {{ 9,  9}, {70, 70}}));
    fm_assert_equal(9, cutʹ(rect, {{10, 10}, {70, 70}}));
#endif
#if 1
    fm_assert_equal(12, cutʹ(rect, {{1, 0}, {50, 50}}));
    fm_assert_equal(12, cutʹ(rect, {{0, 1}, {50, 50}}));
    fm_assert_equal(11, cutʹ(rect, {{1, 1}, {50, 50}}));
#endif
#if 1
    // todo! coverage
#endif
}

} // namespace
} // namespace floormat::Hole

namespace floormat {
void Test::test_hole()
{
    Hole::test1<   0,     0 >();
    Hole::test1<  110,  105 >();
    Hole::test1<   15,  110 >();
    Hole::test1< - 15, -110 >();
    Hole::test1< -110,  -15 >();
}

} // namespace floormat
