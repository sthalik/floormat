#include "app.hpp"
#include "src/hole.hpp"

namespace floormat {
namespace {

using bbox = cut_rectangle_result::bbox;

template<Int x, Int y>
void test1()
{
    static constexpr auto vec = Vector2i{x, y};
    static constexpr auto rect = bbox{{}, {50, 50}};

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
    fm_assert_equal(1, cutʹ(rect, {{ 9,  9}, {70, 70}}));
    fm_assert_equal(1, cutʹ(rect, {{10, 10}, {70, 70}}));
#endif
#if 1
    fm_assert_equal(1, cutʹ(rect, {{1, 0}, {50, 50}}));
    fm_assert_equal(1, cutʹ(rect, {{0, 1}, {50, 50}}));
    fm_assert_equal(2, cutʹ(rect, {{1, 1}, {50, 50}}));
#endif
#if 1
    // todo! coverage
#endif
}

} // namespace

void Test::test_hole()
{
    test1<   0,     0 >();
    test1<  110,  105 >();
    test1<   15,  110 >();
    test1< - 15, -110 >();
    test1< -110,  -15 >();
}

} // namespace floormat
