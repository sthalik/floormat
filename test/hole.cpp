#include "app.hpp"
#include "src/hole.hpp"

namespace floormat {
namespace {

using bbox = cut_rectangle_result::bbox;

constexpr auto cutʹ(bbox rect, bbox hole, Vector2i offset)
{
    auto rectʹ = bbox { rect.position + offset, rect.bbox_size };
    auto holeʹ = bbox { hole.position + offset, hole.bbox_size };
    return cut_rectangle(rectʹ, holeʹ).size;
}

void test1(Vector2i offset)
{
    constexpr auto rect = bbox{{}, {50, 50}};
#if 1
    fm_assert_not_equal(0, cutʹ(rect, {{ 49,   0}, {50, 50}}, offset));
    fm_assert_not_equal(0, cutʹ(rect, {{  0,  49}, {50, 50}}, offset));
    fm_assert_not_equal(0, cutʹ(rect, {{ 49,  49}, {50, 50}}, offset));
#endif
#if 1
    fm_assert_not_equal(0, cutʹ(rect, {{-49,   0}, {50, 50}}, offset));
    fm_assert_not_equal(0, cutʹ(rect, {{  0, -49}, {50, 50}}, offset));
    fm_assert_not_equal(0, cutʹ(rect, {{ 49, -49}, {50, 50}}, offset));
#endif
#if 1
    fm_assert_equal(0, cutʹ(rect, {{50,  0}, {50, 50}}, offset));
    fm_assert_equal(0, cutʹ(rect, {{ 0, 50}, {50, 50}}, offset));
    fm_assert_equal(0, cutʹ(rect, {{50, 50}, {50, 50}}, offset));
#endif
#if 1
    fm_assert_equal(1, cutʹ(rect, {{ 9,  9}, {70, 70}}, offset));
    fm_assert_equal(1, cutʹ(rect, {{10, 10}, {70, 70}}, offset));
#endif
#if 1
    fm_assert_equal(1, cutʹ(rect, {{1, 0}, {50, 50}}, offset));
    fm_assert_equal(1, cutʹ(rect, {{0, 1}, {50, 50}}, offset));
    fm_assert_equal(2, cutʹ(rect, {{1, 1}, {50, 50}}, offset));
#endif
#if 1
    // todo! coverage
#endif
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
}

} // namespace floormat
