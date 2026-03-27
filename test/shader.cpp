#include "app.hpp"
#include "src/point.inl"

namespace floormat::Test {
namespace {

void test_point()
{
    constexpr auto chunk_size = tile_size_xy * (int)TILE_MAX_DIM;

    {
        constexpr auto p = point{{7, -11, 3}, {5, 9}, {13, -17}};
        static_assert(Vector3i(p) == Vector3i{
            chunk_size * 7 + tile_size_xy * 5 + 13,
            chunk_size * -11 + tile_size_xy * 9 - 17,
            tile_size_z * 3,
        });
    }
}
} // namespace

void test_shader()
{
    test_point();
}

} // namespace floormat::Test
