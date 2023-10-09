#include "app.hpp"
#include "src/object.hpp"
#include "bench.hpp"

namespace floormat {

namespace {

point norm(const point& pt, Vector2i delta)
{
    return object::normalize_coords(pt, delta);
};

void test_normalize_point()
{
    point a = { {{  0,   0,    0}, { 0,  0}}, {  0,   0} },
          b = { {{ -1,   1,    2}, { 0, 15}}, {  0,   0} },
          c = { {{ -1,   1,    1}, { 7,  9}}, {  1,  31} },
          d = { {{16384,-16384,2}, {15,  0}}, {  1,   2} };

    fm_assert_equal(norm(a, {}),           point{{{  0,   0,    0}, { 0,  0}}, {  0,   0} });
    fm_assert_equal(norm(b, {}),           point{{{ -1,   1,    2}, { 0, 15}}, {  0,   0} });
    fm_assert_equal(norm(b, {  1,  -1} ),  point{{{ -1,   1 ,   2}, { 0, 15}}, {  1,  -1} });
    fm_assert_equal(norm(b, { -65,  65 }), point{{{ -2,   2,    2}, {15,  0}}, { -1,   1} });
    fm_assert_equal(norm(c, {  30, -62 }), point{{{ -1,   1,    1}, { 7,  9}}, { 31, -31} });
    fm_assert_equal(norm(c, {1024, 1024}), point{{{  0,   2,    1}, { 7,  9}}, {  1,  31} });
    fm_assert_equal(norm(d, {2048, 1087}), point{{{16386,-16383,2}, {15,  1}}, {  1,   1} });
}

} // namespace

void test_app::test_coords()
{
    test_normalize_point();
}

} // namespace floormat
