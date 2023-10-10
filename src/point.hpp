#pragma once
#include "global-coords.hpp"
#include <Corrade/Utility/Move.h>
#include <compare>

namespace floormat {

// todo pack this within 8 bytes
struct point
{
    global_coords coord;
    Vector2b offset;

    constexpr bool operator==(const point&) const = default;
    friend constexpr std::strong_ordering operator<=>(const point& a, const point& b) noexcept;

    friend Debug& operator<<(Debug& dbg, const point& pt);
};

constexpr std::strong_ordering operator<=>(const point& p1, const point& p2) noexcept
{
    auto c1 = Vector3i(p1.coord), c2 = Vector3i(p2.coord);

    if (auto val = c1.z() <=> c2.z(); val != std::strong_ordering::equal)
        return val;
    if (auto val = c1.y() <=> c2.y(); val != std::strong_ordering::equal)
        return val;
    if (auto val = c1.x() <=> c2.x(); val != std::strong_ordering::equal)
        return val;
    if (auto val = p1.offset.y() <=> p2.offset.y(); val != std::strong_ordering::equal)
        return val;
    if (auto val = p1.offset.x() <=> p2.offset.x(); val != std::strong_ordering::equal)
        return val;

    return std::strong_ordering::equal;
}

struct packed_point
{
    uint64_t cx : 12, cy : 12, cz : 4,
             tx : 8, ty : 8,
             ox : 4, oy : 4;
};

} // namespace floormat
