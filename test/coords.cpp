#include "app.hpp"
#include "src/object.hpp"

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

    fm_assert_equal(point{{{  0,   0,    0}, { 0,  0}}, {  0,   0} }, norm(a, {}          ));
    fm_assert_equal(point{{{ -1,   1,    2}, { 0, 15}}, {  0,   0} }, norm(b, {          }));
    fm_assert_equal(point{{{ -1,   1 ,   2}, { 0, 15}}, {  1,  -1} }, norm(b, {  1,  -1  }));
    fm_assert_equal(point{{{ -2,   2,    2}, {15,  0}}, { -1,   1} }, norm(b, { -65,  65 }));
    fm_assert_equal(point{{{ -1,   1,    1}, { 7,  9}}, { 31, -31} }, norm(c, {  30, -62 }));
    fm_assert_equal(point{{{  0,   2,    1}, { 7,  9}}, {  1,  31} }, norm(c, {1024, 1024}));
    fm_assert_equal(point{{{16386,-16383,2}, {15,  1}}, {  1,   1} }, norm(d, {2048, 1087}));
}

} // namespace

void test_app::test_coords()
{
    test_normalize_point();
}

} // namespace floormat
