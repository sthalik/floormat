#include "app.hpp"
#include "src/point.inl"

namespace floormat {

namespace {

point norm(const point& pt, Vector2i delta)
{
    return point::normalize_coords(pt, delta);
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

void test_point()
{
    constexpr auto c = tile_size_xy * (int32_t)TILE_MAX_DIM;
    constexpr auto t = tile_size_xy;
    constexpr auto h = tile_size_xy/2;

    constexpr auto v1 = Vector3i{-h, -h, 0};
    constexpr auto p1 = point{v1};
    fm_assert_equal(v1, Vector3i{p1});

    constexpr auto v2 = v1 - Vector3i{1, 1, 0};
    constexpr auto p2 = point{v2};
    fm_assert_equal(v2, Vector3i{p2});

    constexpr auto v3 = Vector3i{c * 128 + t * ((int32_t)TILE_MAX_DIM-1) + tile_size_xy/2 - 1, c * 42 + t * 3 - h/2, tile_size_z * 10};
    constexpr auto p3 = point{v3};
    fm_assert_equal(v3, Vector3i{p3});

    constexpr auto v4 = most_positive_point;
    constexpr auto p4 = point{v4};
    fm_assert_equal(v4, Vector3i{p4});

#if 0
    DBG << "";
    DBG << "";
    DBG << p1;
    DBG << p2;
    DBG << p3;
    DBG << "";
#endif
}

} // namespace

void Test::test_coords()
{
    test_normalize_point();
    test_point();
}

} // namespace floormat
