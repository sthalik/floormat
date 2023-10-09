#include "app.hpp"
#include "src/object.hpp"

namespace floormat {

namespace {

constexpr auto norm = [](const point& pt, Vector2i delta) { return object::normalize_coords(pt, delta); };

void test_normalize_point()
{
    auto a = point{{{  0,   0,  0}, { 0,  0}}, {  0,   0} },
         b = point{{{ -1,   1,  0}, { 0, 15}}, {  0,   0} },
         c = point{{{ -1,   1,  1}, { 0,  0}}, {  1,  31} },
         d = point{{{  1,   0,  1}, {15, 15}}, {-31,  31} },
         e = point{{{  1,   0,  1}, {15, 15}}, {-31,  31} },
         f = point{{{16384, -16384, 2}, {15, 0}}, {1, 2}  };

    fm_assert_equal(norm(a, {}),          point{{{  0,   0,   0}, { 0,  0}}, {  0,   0} });
    fm_assert_equal(norm(b, {  1,  -1}),  point{{{ -1,   1 ,  0}, { 0, 15}}, {  1,  -1} });
    fm_assert_equal(norm(b, { -65,  65}), point{{{ -2,   2,   0}, {15,  0}}, { -1,   1} });
}

} // namespace

void test_app::test_coords()
{
    test_normalize_point();
}

} // namespace floormat
