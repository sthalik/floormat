#pragma once
#include "global-coords.hpp"
#include "compat/defs.hpp"
#include <compare>
#include <type_traits>
#include <Corrade/Utility/StlForwardTuple.h>

namespace floormat { struct point; }

template<> struct std::tuple_size<floormat::point> : std::integral_constant<floormat::size_t, 2> {};

template<floormat::size_t N> struct std::tuple_element<N, floormat::point>;

template<> struct std::tuple_element<0, floormat::point> { using type = floormat::global_coords; };
template<> struct std::tuple_element<1, floormat::point> { using type = Magnum::Vector2b; };

namespace floormat {

struct point
{
    int16_t cx = 0, cy = 0;
    int8_t cz = 0;
    local_coords tile;
    Vector2b offset;

    constexpr point();
    constexpr point(global_coords coord, Vector2b offset);
    constexpr point(chunk_coords_ coord, local_coords tile, Vector2b offset);
    fm_DECLARE_DEFAULT_COPY_ASSIGNMENT(point);

    constexpr bool operator==(const point&) const noexcept = default;
    friend constexpr std::strong_ordering operator<=>(const point& a, const point& b);

    constexpr global_coords coord() const;
    constexpr chunk_coords chunk() const;
    constexpr chunk_coords_ chunk3() const;
    constexpr local_coords local() const;
    friend Debug& operator<<(Debug& dbg, const point& pt);
};

constexpr std::strong_ordering operator<=>(const point& p1, const point& p2)
{
    if (auto val = p1.cz <=> p2.cz; val != std::strong_ordering::equal) return val;
    if (auto val = p1.cy <=> p2.cy; val != std::strong_ordering::equal) return val;
    if (auto val = p1.cx <=> p2.cx; val != std::strong_ordering::equal) return val;
    if (auto val = p1.tile.y <=> p2.tile.y; val != std::strong_ordering::equal) return val;
    if (auto val = p1.tile.x <=> p2.tile.x; val != std::strong_ordering::equal) return val;
    if (auto val = p1.offset.y() <=> p2.offset.y(); val != std::strong_ordering::equal) return val;
    if (auto val = p1.offset.x() <=> p2.offset.x(); val != std::strong_ordering::equal) return val;
    return std::strong_ordering::equal;
}

constexpr point::point() = default;
constexpr point::point(global_coords coord, Vector2b offset) : point{coord.chunk3(), coord.local(), offset} { }

constexpr point::point(chunk_coords_ coord, local_coords tile, Vector2b offset) :
    cx{coord.x}, cy{coord.y}, cz{coord.z}, tile{tile}, offset{offset}
{}

constexpr global_coords point::coord() const { return {{cx, cy}, tile, cz}; }
constexpr chunk_coords_ point::chunk3() const { return {cx, cy, cz}; }
constexpr chunk_coords point::chunk() const { return {cx, cy}; }
constexpr local_coords point::local() const { return tile; }

template<size_t N> std::tuple_element_t<N, point> constexpr get(point pt) {
    static_assert(N < 2);
    if constexpr(N == 0)
        return global_coords{{pt.cx, pt.cy}, pt.tile, pt.cz};
    if constexpr(N == 1)
        return pt.offset;
    return {};
}

} // namespace floormat
