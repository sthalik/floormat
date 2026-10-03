#include "app.hpp"
#include "src/point.inl"
#include "compat/floor-divmod.hpp"
#include "shaders/shader.hpp"

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
static_assert(floor_divmod<64>(Vector2i{-1, 64}) == Pair<Vector2i, Vector2i>{{-1, 1}, {63, 0}});

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

Vector2i camera_centered_on(Vector3i world)
{
    return -floor_divmod<2>(tile_shader::projectʹ(world)).first();
}

void test_pixel_to_point()
{
    constexpr int8_t z = 3;
    constexpr auto zpx = z*tile_size_z;

    {
        // world {31.5, 30.5}: x rounds up into the next tile
        const auto p = tile_shader::pixel_to_point({321, 271 - zpx}, {640, 480}, {}, z);
        fm_assert_equal(point{{0, 0, z}, {1, 0}, {-32, 31}}, p);
    }

    const Vector2i wins[] = { {640, 480}, {641, 481}, {1920, 1081}, {1279, 720}, };
    constexpr auto a = 1 << 23, b = 1 << 24;
    constexpr auto lo = most_negative_point_xy + 48, hi = most_positive_point_xy - 48;
    constexpr auto c = chunk_size<int>;
    const Vector3i centers[] = {
        {0, 0, 0}, {c - half_tile<int>, -half_tile<int>, zpx}, {-c - 33, c + 31, zpx},
        {lo, lo, 0}, {hi, hi, zpx}, {lo, hi, 0}, {hi, lo, zpx}, {c + 5, -c - 7, -tile_size_z},
    };

    const auto check = [](Vector2i pixel, Vector2i win, Vector2i cam, int8_t zl) {
        // Exact in double: every term is a multiple of 1/2 below 2^30.
        const auto zp = zl*tile_size_z;
        const auto s = Vector2d(pixel - win/2 - cam) + Vector2d{0., (double)zp};
        const auto w = tile_shader::unproject(s*.5);
        const auto expected = Vector3i{Vector2i(Math::floor(w + Vector2d{.5})), zp};
        fm_assert_equal(expected, Vector3i(tile_shader::pixel_to_point(pixel, win, cam, zl)));
    };

    for (auto win : wins)
    {
        const Vector2i cams[] = {
            {}, {1, -1}, {a+1, -a+1}, {-a-1, a-1}, {b, -b}, {-b, b},
        };
        for (auto cam : cams)
            for (int y = -32; y < 32; y++)
                for (int x = -32; x < 32; x++)
                    check(win/2 + Vector2i{x, y}, win, cam, z);

        for (auto world : centers)
        {
            const auto cam = camera_centered_on(world);
            const auto zl = int8_t(world.z()/tile_size_z);
            fm_assert_equal(point{world}, tile_shader::pixel_to_point(win/2, win, cam, zl));
            fm_assert_equal(win/2, tile_shader::point_to_pixelʹ(world, win, cam));
            const auto half = float((world.x() + world.y()) & 1)*.5f;
            fm_assert_equal(Vector2(win/2) + Vector2{0, half}, tile_shader::point_to_pixel(world, win, cam));
            for (int y = -32; y < 32; y++)
                for (int x = -32; x < 32; x++)
                {
                    check(win/2 + Vector2i{x, y}, win, cam, zl);
                    const auto w = world + Vector3i{x, y, 0};
                    fm_assert_equal(point{w}, tile_shader::pixel_to_point(tile_shader::point_to_pixelʹ(w, win, cam), win, cam, zl));
                }
        }
    }
}

} // namespace

void Test::test_coords()
{
    test_normalize_point();
    test_point();
    test_pixel_to_point();
}

} // namespace floormat
