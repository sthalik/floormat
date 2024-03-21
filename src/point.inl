#pragma once
#include "point.hpp"
#include "src/tile-constants.hpp"
#include <mg/Functions.h>

namespace floormat {

constexpr uint32_t point::distance(point a, point b)
{
    Vector2i dist;
    dist += (a.coord() - b.coord())*iTILE_SIZE2;
    dist += Vector2i(a.offset()) - Vector2i(b.offset());
    return (uint32_t)Math::ceil(Math::sqrt(Vector2(dist).dot()));
}

constexpr uint32_t point::distance_l2(point a, point b)
{
    Vector2i dist;
    dist += (a.coord() - b.coord())*iTILE_SIZE2;
    dist += Vector2i(a.offset()) - Vector2i(b.offset());
    return (uint32_t)Math::abs(dist).sum();
}

constexpr std::strong_ordering operator<=>(const point& p1, const point& p2)
{
    if (auto val = p1.cz <=> p2.cz; val != std::strong_ordering::equal) return val;
    if (auto val = p1.cy <=> p2.cy; val != std::strong_ordering::equal) return val;
    if (auto val = p1.cx <=> p2.cx; val != std::strong_ordering::equal) return val;
    if (auto val = p1.tile.y <=> p2.tile.y; val != std::strong_ordering::equal) return val;
    if (auto val = p1.tile.x <=> p2.tile.x; val != std::strong_ordering::equal) return val;
    if (auto val = p1._offset.y() <=> p2._offset.y(); val != std::strong_ordering::equal) return val;
    if (auto val = p1._offset.x() <=> p2._offset.x(); val != std::strong_ordering::equal) return val;
    return std::strong_ordering::equal;
}

constexpr Vector2i operator-(const point& p1, const point& p2)
{
    fm_debug_assert(p1.cz == p2.cz);
    Vector2i sum;
    sum += iTILE_SIZE2 * TILE_MAX_DIM * (Vector2i(p1.cx, p1.cy) - Vector2i(p2.cx, p2.cy));
    sum += iTILE_SIZE2 * (Vector2i(p1.tile.x, p1.tile.y) - Vector2i(p2.tile.x, p2.tile.y));
    sum += Vector2i(p1._offset) - Vector2i(p2._offset);
    return sum;
}

} // namespace floormat
