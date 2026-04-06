#include "point.inl"
#include "tile-constants.hpp"
#include <bit>
#include <cr/StructuredBindings.h>

namespace floormat {

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

point operator+(point pt, Vector2i delta) { return point::normalize_coords(pt, delta); }
point operator+(Vector2i delta, point pt) { return point::normalize_coords(pt, delta); }

namespace {
template<int tile_size>
constexpr inline Pair<int, int8_t> normalize_coord(const int8_t cur, const int new_off)
{
    if constexpr(tile_size > 0 && (tile_size & tile_size-1) == 0)
    {
        constexpr int half = tile_size / 2;
        constexpr int mask = tile_size - 1;
        constexpr int shift = std::countr_zero((unsigned)tile_size);
        const int val = cur + new_off + half;
        return { val >> shift, (int8_t)((val & mask) - half) };
    }
    else
    {
        constexpr int8_t half_tile = tile_size/2;
        const int tmp = cur + new_off;
        auto x = (int8_t)(tmp % tile_size);
        auto t = tmp / tile_size;
        auto a = Math::abs(x);
        auto s = Math::sign(x);
        bool b = x >= half_tile | x < -half_tile;
        auto tmask = -(int)b;
        auto xmask = (int8_t)-(int8_t)b;
        t += s & tmask;
        x = (int8_t)((tile_size - a)*-s) & xmask | (int8_t)(x & ~xmask);
        return { t, x };
    }
}

} // namespace

point point::normalize_coords(global_coords coord, Vector2b cur, Vector2i new_off)
{
    auto [cx, ox] = normalize_coord<iTILE_SIZE2.x()>(cur.x(), new_off.x());
    auto [cy, oy] = normalize_coord<iTILE_SIZE2.y()>(cur.y(), new_off.y());
    coord += Vector2i(cx, cy);
    return { coord, { ox, oy }, };
}

point point::normalize_coords(point pt, Vector2i delta)
{
    return normalize_coords(pt.coord(), pt.offset(), delta);
}


namespace {
namespace krap {

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
