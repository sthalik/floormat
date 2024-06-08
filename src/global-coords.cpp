#include "global-coords.hpp"
#include "point.inl"
#include "compat/hash.hpp"
#include <array>
#include <algorithm>

namespace floormat {

Debug& operator<<(Debug& dbg, const chunk_coords& c)
{
    dbg << "";
    const auto flags = dbg.flags();

    dbg.setFlags(flags | Debug::Flag::NoSpace);
    dbg << "chunk{" << c.x << "," << c.y << "}";

    dbg.setFlags(flags);
    return dbg;
}

Debug& operator<<(Debug& dbg, const chunk_coords_& c)
{
    dbg << "";
    const auto flags = dbg.flags();

    dbg.setFlags(flags | Debug::Flag::NoSpace);
    dbg << "chunk{" << c.x << "," << c.y;
    if (c.z != 0)
        dbg << "," << c.z;
    dbg << "}";

    dbg.setFlags(flags);
    return dbg;
}

size_t global_coords::hash() const noexcept
{
    static_assert(sizeof *this == 4 + 4);
    return hash_buf(this, sizeof *this);
}

namespace {

static_assert(sizeof(decltype(local_coords::x))*8 == 8);
static_assert(sizeof(decltype(chunk_coords::x))*8 == 16);
static_assert(std::is_same_v<decltype(local_coords::x), decltype(local_coords::y)>);
static_assert(std::is_same_v<decltype(chunk_coords::x), decltype(chunk_coords::y)>);

static_assert(std::is_same_v<decltype(chunk_coords::x), decltype(chunk_coords::y)>);

static_assert(TILE_MAX_DIM == (1 << 4));

static_assert(global_coords{(int)TILE_MAX_DIM-1, (int)TILE_MAX_DIM-1, 0}.chunk() == global_coords{}.chunk());
static_assert(global_coords{(int)TILE_MAX_DIM-1, (int)TILE_MAX_DIM, 0}.chunk() == chunk_coords{0, 1});
static_assert(global_coords{(1u + (1<<15)) << 4 | 3, (2u + (1<<15)) << 4 | 4, nullptr} == global_coords{{1, 2}, {3, 4}, -1});

static_assert(global_coords{-123, 456, 1}.z() == 1);
static_assert(global_coords{-123, 511, 5}.chunk() == chunk_coords{-8, 31});

static_assert(chunk_coords_{(char)10, (char)20, (char)30} + Vector3i(1, 2, 3) == chunk_coords_{(char)11, (char)22, (char)33});
static_assert(chunk_coords_{(char)11, (char)22, (char)33} - Vector3i(1, 2, 3) == chunk_coords_{(char)10, (short)20, (short)30});

static_assert(chunk_coords_{(short)10, (short)20, (char)30} + Vector2i(1, 2) == chunk_coords_{(short)11, (short)22, (char)30});
static_assert(chunk_coords_{(short)11, (short)22, (char)30} - Vector2i(1, 2) == chunk_coords_{(short)10, (short)20, (char)30});

constexpr auto g1 = global_coords{{1, 2, 0}, {3, 0}};
constexpr auto g2 = global_coords{{1, 1, 0}, {3, TILE_MAX_DIM-1}};

static_assert(g1 - g2 == Vector2i(0, 1));
static_assert((g1 + Vector2i(0, -1)).chunk() == g2.chunk());
static_assert(g1 + Vector2i(0, -1) == g2);

constexpr bool test_comparison1()
{
    auto a = point{{2, 8, 2}, {0, 0}};
    auto b = point{{1, 1, 2}, {9, 1}};
    auto c = point{{1, 0, 2}, {1, 2}};
    auto d = point{{1, 9, 1}, {1, 9}};
    auto e = point{{3, 1, 2}, {0, 0}};
    auto f = point{{9, 8, 0}, {9, 9}};
    auto g = point{{1, 9, 1}, {9, 0}};

    const auto sorted = std::array{
        f, g, d, c, b, e, a,
    };
    auto array1 = std::array{
        a, b, c, d, e, f, g,
    };
    auto array2 = std::array {
        a, c, e, g, b, d, f,
    };

    std::sort(array1.begin(), array1.end());
    fm_assert(array1 == sorted);

    std::sort(array2.begin(), array2.end());
    fm_assert(array2 == sorted);

    return true;
}

static_assert(test_comparison1());

} // namespace

} // namespace floormat
