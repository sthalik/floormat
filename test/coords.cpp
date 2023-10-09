#include "app.hpp"
#include "src/object.hpp"

namespace floormat {

namespace {

constexpr auto norm = [](const point& pt, Vector2i delta) { return object::normalize_coords(pt, delta); };

void test_normalize_point()
{
    auto a = point{{{ 0,  0,  0}, { 0,  0}}, {  0,   0} },
         b = point{{{ 0, -1,  0}, {15, 15}}, {  0,   0} },
         c = point{{{-1,  1,  1}, { 0,  0}}, {  1,  31} },
         d = point{{{ 1,  0,  1}, {15, 15}}, {-31,  31} },
         e = point{{{ 1,  0,  1}, {15, 15}}, {-31,  31} };

    fm_assert_equal(norm(a, {}), point{{{ 0,  0,  0}, { 0,  0}}, {  0,   0} });
}

} // namespace

void test_app::test_coords()
{
    test_normalize_point();
}

} // namespace floormat
