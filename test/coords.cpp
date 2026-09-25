#include "app.hpp"
#include "src/point.inl"
#include "compat/floor-divmod.hpp"

namespace floormat {

namespace {

using P2 = Pair<int, int>;
static_assert(floor_divmod<1024>(-1)    == P2{-1, 1023});
static_assert(floor_divmod<1024>(-1024) == P2{-1, 0});
static_assert(floor_divmod<1024>(-1025) == P2{-2, 1023});
static_assert(floor_divmod<1024>(1023)  == P2{0, 1023});
static_assert(floor_divmod<1024>(1024)  == P2{1, 0});
static_assert(floor_divmod<192>(-1)     == P2{-1, 191});
static_assert(floor_divmod<192>(-192)   == P2{-1, 0});
static_assert(floor_divmod<192>(-193)   == P2{-2, 191});
static_assert(floor_divmod<192>(191)    == P2{0, 191});
static_assert(floor_divmod<192>(192)    == P2{1, 0});

static_assert(point{Vector3i{-33, 991, 0}} == point{{-1, 0, 0}, {15, 15}, {31, 31}});
static_assert(point{Vector3i{-32, 992, 0}} == point{{0, 1, 0}, {0, 0}, {-32, -32}});

point norm(const point& pt, Vector2i delta)
{
    return point::normalize_coords(pt, delta);
};

void test_normalize_point()
{
    point a = { {{  0,   0,    0}, { 0,  0}}, {  0,   0} },
          b = { {{ -1,   1,    2}, { 0, 15}}, {  0,   0} },
          c = { {{ -1,   1,    1}, { 7,  9}}, {  1,  31} },
          d = { {{ 8192, -8192,2}, {15,  0}}, {  1,   2} };

    fm_assert_equal(point{{{  0,   0,    0}, { 0,  0}}, {  0,   0} }, norm(a, {}          ));
    fm_assert_equal(point{{{ -1,   1,    2}, { 0, 15}}, {  0,   0} }, norm(b, {          }));
    fm_assert_equal(point{{{ -1,   1 ,   2}, { 0, 15}}, {  1,  -1} }, norm(b, {  1,  -1  }));
    fm_assert_equal(point{{{ -2,   2,    2}, {15,  0}}, { -1,   1} }, norm(b, { -65,  65 }));
    fm_assert_equal(point{{{ -1,   1,    1}, { 7,  9}}, { 31, -31} }, norm(c, {  30, -62 }));
    fm_assert_equal(point{{{  0,   2,    1}, { 7,  9}}, {  1,  31} }, norm(c, {1024, 1024}));
    fm_assert_equal(point{{{ 8194, -8191,2}, {15,  1}}, {  1,   1} }, norm(d, {2048, 1087}));
}

void test_point()
{
    constexpr auto c = chunk_size<int32_t>;
    constexpr auto t = tile_size_xy;
    constexpr auto h = half_tile<int32_t>;

    constexpr auto v1 = Vector3i{-h, -h, 0};
    constexpr auto p1 = point{v1};
    fm_assert_equal(v1, Vector3i{p1});

    constexpr auto v2 = v1 - Vector3i{1, 1, 0};
    constexpr auto p2 = point{v2};
    fm_assert_equal(v2, Vector3i{p2});

    constexpr auto v3 = Vector3i{c * 128 + t * ((int32_t)TILE_MAX_DIM-1) + h - 1, c * 42 + t * 3 - h/2, tile_size_z * 10};
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

void test_from_fractional_tile()
{
    constexpr int8_t z = 3;

    for (int w = -2*chunk_size<int>; w <= 2*chunk_size<int>; w++)
    {
        const auto px = Vector2i{w, -w - 13};
        const auto tile = Vector2d(px) / tile_size<Vector2d> + Vector2d{.5};
        fm_assert_equal(Vector3i{px, z*tile_size_z}, Vector3i(point::from_fractional_tile(tile, z)));
    }

    {
        // both round to a whole tile in float
        const auto tile = Vector2d{3 - 1e-8, -5 + 1e-8};
        const auto px = (tile - Vector2d{.5}) * tile_size<Vector2d>;
        const auto p = Vector3i(point::from_fractional_tile(tile, z));
        fm_assert((Math::abs(Vector2d(p.xy()) - px) <= Vector2d{1}).all());
    }
}

} // namespace

void Test::test_coords()
{
    test_normalize_point();
    test_point();
    test_from_fractional_tile();
}

} // namespace floormat
