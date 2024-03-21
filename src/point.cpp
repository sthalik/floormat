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

constexpr auto Ch = iTILE_SIZE2 * TILE_MAX_DIM;

static_assert(point::distance_l2(
    point{{ 1,  2, 0}, {3, 4}, {32, 32}},
    point{{ 0,  0, 0}, {0, 0}, {32, 32}}
) == (uint32_t)Math::abs((Vector2i(1, 2)*Ch + Vector2i{3, 4} * iTILE_SIZE2 + Vector2i{0, 0}).sum()));

static_assert(point::distance_l2(
    point{{ 0,  0, 0}, {0, 0}, {30, 30}},
    point{{ 1,  2, 0}, {3, 4}, {31, 31}}
) == (uint32_t)Math::abs((Vector2i(1, 2)*Ch + Vector2i{3, 4} * iTILE_SIZE2 + Vector2i{1, 1}).sum()));

static_assert(point::distance_l2(
    point{{ 2,  3, 0}, {4, 5}, {32, 32}},
    point{{ 1,  2, 0}, {3, 4}, {31, 31}}
) == (uint32_t)Math::abs((Vector2i(1, 1)*Ch + Vector2i{1, 1} * iTILE_SIZE2 + Vector2i{1, 1}).sum()));

static_assert(point::distance_l2(
    point{{ 1,  2, 0}, {3, 4}, {31, 31}},
    point{{ 2,  3, 0}, {4, 5}, {32, 32}}
) == (uint32_t)Math::abs((Vector2i(1, 1)*Ch + Vector2i{1, 1} * iTILE_SIZE2 + Vector2i{1, 1}).sum()));

namespace krap {

constexpr auto T = iTILE_SIZE2;
using V2 = Vector2i;
using P = point;

static_assert(P{{}, {}, {}}                  - P{{}, {}, {}}                      == V2{});
static_assert(P{{}, {}, {}}                  - P{{}, {1, 0}, {}}                  == T * V2{-1, 0}                                  );
static_assert(P{{}, {2, 3}, {}}              - P{{}, {4, 6}, {}}                  == T * V2{-2, -3}                                 );
static_assert(P{{}, {6, 4}, {}}              - P{{}, {2, 3}, {}}                  == T * V2{4, 1}                                   );
static_assert(P{{7, 8, 0}, {6, 4}, {}}       - P{{}, {2, 3}, {}}                  == T * V2{4, 1} + Ch * V2{7, 8}                   );
static_assert(P{{7, 8, 0}, {6, 4}, {}}       - P{{9, -11, 0}, {2, 3}, {}}         == T * V2{4, 1} + Ch * V2{-2, 19}                 );
static_assert(P{{7, 8, 1}, {6, 4}, {24, 16}} - P{{9, -11, 1}, {2, 3}, {-16, 24}}  == T * V2{4, 1} + Ch * V2{-2, 19} + V2{40, -8}    );

} // namespace krap


} // namespace

} // namespace floormat
