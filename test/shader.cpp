#include "app.hpp"
#include "src/point.inl"
#include "src/depth.hpp"
#include "shaders/shader.hpp"
#include "shaders/texture-unit-cache.hpp"

namespace floormat::Test {
namespace {

void test_point()
{
    constexpr auto chunk_size = tile_size_xy * (int)TILE_MAX_DIM;
    constexpr auto tile_size  = Vector3i{tile_size_xy, tile_size_xy, tile_size_z};

    {
        constexpr auto my_chunk = chunk_size * Vector3i{1, 2, 0};
        constexpr auto my_tile  = tile_size * Vector3i{4, 5, 0};
        constexpr auto my_pixel = my_chunk + my_tile + Vector3i{6, 7, 0};

        constexpr auto a = point {{1, 2, 0}, {4, 5}, {6, 7}};
        constexpr auto Va = Depth::value_at(a);
        fm_assert(Vector3i(a) == my_pixel);

        constexpr auto b = point {{1, 2, -1}, {4, 5}, {6, 7}};

        constexpr auto Vb = Depth::value_at(b);
        fm_assert(Va > Vb);

        constexpr auto c = point {{1, 2, -1}, {4, 5}, {3, 10}};
        constexpr auto Vc = Depth::value_at(c);
        fm_assert(Vc == Vb);
        fm_assert_equal(Vector3i{1  * chunk_size + 4 * tile_size_xy + 3,
                                 2  * chunk_size + 5 * tile_size_xy + 10,
                                -1  * tile_size_z},
                        Vector3i(c));

        constexpr auto d = point {{1, 2, 0}, {4, 5}, {6, 5}};
        constexpr auto Vd = Depth::value_at(d);
        fm_assert(Vd < Va);
        fm_assert(Vd > Vb);

        constexpr auto e = point {{1, 2, -1}, {4, 5}, {6, 5}};
        constexpr auto Ve = Depth::value_at(e);
        fm_assert(Ve < Vb);
    }

    {
        constexpr auto a = point {{1, 2, 0}, {1, 2}, {}};
        constexpr auto Va = Depth::value_at(a);
        constexpr auto b = point {{2, 1, 0}, {2, 1}, {}};
        constexpr auto Vb = Depth::value_at(b);
        fm_assert(Va == Vb);
        constexpr auto c = point {{2, 1, 0}, {2, 1}, {1, 0}};
        constexpr auto Vc = Depth::value_at(c);
        fm_assert(Vc > Vb);
        fm_assert(Va < Vc);
        constexpr auto d = point {{2, 1, 0}, {1, 1}, {}};
        constexpr auto Vd = Depth::value_at(d);
        fm_assert(Vd < Vc);
        constexpr auto e = point {{2, 1, 0}, {1, 1}, {1, 0}};
        constexpr auto Ve = Depth::value_at(e);
        fm_assert(Ve > Vd);
        fm_assert(Ve < Vc);
    }
    {
        constexpr auto a = point{{11, 22, 0}, {4, 5}, {6, 7}};
        constexpr auto b = point{{22, 11, 0}, {4, 5}, {6, 7}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(b));
        constexpr auto c = point{{22, 11, 0}, {5, 4}, {6, 7}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(c));
        constexpr auto d = point{{11, 22, 0}, {4, 5}, {7, 6}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(d));
        constexpr auto e = point{{13, 20, 0}, {4, 5}, {6, 7}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(e));
        constexpr auto f = point{{13, 20, 0}, {2, 7}, {6, 7}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(f));
        constexpr auto g = point{{13, 20, 0}, {2, 7}, {8, 5}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(g));
    }

    {
        constexpr auto p = point{{7, -11, 3}, {5, 9}, {13, -17}};
        static_assert(Vector3i(p) == Vector3i{
            chunk_size * 7 + tile_size_xy * 5 + 13,
            chunk_size * -11 + tile_size_xy * 9 - 17,
            tile_size_z * 3,
        });
    }
}

void test_shader_program()
{
    texture_unit_cache tuc;
    tile_shader S{tuc};
}
} // namespace

void test_shader()
{
    test_point();
    test_shader_program();
}

} // namespace floormat::Test
