#pragma once
#include "global-coords.hpp"
#include <compare>
#include <type_traits>
#include <Corrade/Utility/StlForwardTupleSizeElement.h>

namespace floormat { struct point; }

template<> struct std::tuple_size<floormat::point> : std::integral_constant<floormat::size_t, 2> {};

template<floormat::size_t N> struct std::tuple_element<N, floormat::point>;

template<> struct std::tuple_element<0, floormat::point> { using type = floormat::global_coords; };
template<> struct std::tuple_element<1, floormat::point> { using type = Magnum::Vector2b; };

namespace floormat {

struct point
{
    constexpr point();
    constexpr point(global_coords coord, Vector2b offset);
    constexpr point(chunk_coords_ coord, local_coords tile, Vector2b offset);

    constexpr bool operator==(const point&) const noexcept;
    friend constexpr std::strong_ordering operator<=>(const point& a, const point& b);
    friend constexpr Vector2i operator-(const point& p1, const point& p2);

    constexpr global_coords coord() const;
    constexpr chunk_coords chunk() const;
    constexpr chunk_coords_ chunk3() const;
    constexpr local_coords local() const;
    constexpr Vector2b offset() const;
    template<size_t N> std::tuple_element_t<N, point> constexpr get() const;

    size_t hash() const;
    friend Debug& operator<<(Debug& dbg, const point& pt);

    static constexpr uint32_t distance(point a, point b);
    static constexpr uint32_t distance_l2(point a, point b);

    friend constexpr Vector2i operator-(const point& p1, const point& p2);

private:
    int16_t cx = 0, cy = 0;
    int8_t cz = 0;
    local_coords tile;
    Vector2b _offset;
};

constexpr point::point() = default;
constexpr point::point(global_coords coord, Vector2b offset) : point{coord.chunk3(), coord.local(), offset} {}
constexpr point::point(chunk_coords_ coord, local_coords tile, Vector2b offset) :
    cx{coord.x}, cy{coord.y}, cz{coord.z}, tile{tile}, _offset{offset}
{}
constexpr bool point::operator==(const point&) const noexcept = default;

constexpr global_coords point::coord() const { return {{cx, cy}, tile, cz}; }
constexpr chunk_coords_ point::chunk3() const { return {cx, cy, cz}; }
constexpr chunk_coords point::chunk() const { return {cx, cy}; }
constexpr local_coords point::local() const { return tile; }
constexpr Vector2b point::offset() const { return _offset; }

template<size_t N> std::tuple_element_t<N, point> constexpr point::get() const
{
    static_assert(N < 2);
    if constexpr(N == 0)
        return global_coords{{cx, cy}, tile, cz};
    if constexpr(N == 1)
        return _offset;
    return {};
}

} // namespace floormat
