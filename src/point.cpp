#include "compat/int-hash.hpp"
#include "point.inl"
#include "tile-constants.hpp"

namespace floormat {

size_t point::hash() const
{
    constexpr size_t size = 2 * 2 + 1 + 1 + 2;
    static_assert(sizeof *this == size);
#ifdef FLOORMAT_64
    static_assert(sizeof nullptr > 4);
    return hash_64(this, sizeof *this);
#else
    static_assert(sizeof nullptr == 4);
    return hash_32(this, sizeof *this);
#endif
}

Debug& operator<<(Debug& dbg, const point& pt)
{
    dbg << "";
    const auto flags = dbg.flags();
    dbg.setFlags(flags | Debug::Flag::NoSpace);

    auto c = Vector3i(pt.chunk3());
    auto t = Vector2i(pt.local());
    auto o = pt.offset();

    dbg << "point{";
    dbg << "{" << c.x() << "," << c.y() << "," << c.z() << "},";
    dbg << "{" << t.x() << "," << t.y() << "},";
    dbg << "{" << o.x() << "," << o.y() << "}}";

    dbg.setFlags(flags);
    return dbg;
}

namespace {

constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;

static_assert(point::distance_l2(
    point{{ 1,  2, 0}, {3, 4}, {32, 32}},
    point{{ 0,  0, 0}, {0, 0}, {32, 32}}
) == (uint32_t)Math::abs((Vector2i(1, 2)*chunk_size + Vector2i{3, 4} * iTILE_SIZE2 + Vector2i{0, 0}).sum()));

static_assert(point::distance_l2(
    point{{ 0,  0, 0}, {0, 0}, {30, 30}},
    point{{ 1,  2, 0}, {3, 4}, {31, 31}}
) == (uint32_t)Math::abs((Vector2i(1, 2)*chunk_size + Vector2i{3, 4} * iTILE_SIZE2 + Vector2i{1, 1}).sum()));

static_assert(point::distance_l2(
    point{{ 2,  3, 0}, {4, 5}, {32, 32}},
    point{{ 1,  2, 0}, {3, 4}, {31, 31}}
) == (uint32_t)Math::abs((Vector2i(1, 1)*chunk_size + Vector2i{1, 1} * iTILE_SIZE2 + Vector2i{1, 1}).sum()));

static_assert(point::distance_l2(
    point{{ 1,  2, 0}, {3, 4}, {31, 31}},
    point{{ 2,  3, 0}, {4, 5}, {32, 32}}
) == (uint32_t)Math::abs((Vector2i(1, 1)*chunk_size + Vector2i{1, 1} * iTILE_SIZE2 + Vector2i{1, 1}).sum()));

} // namespace

} // namespace floormat
