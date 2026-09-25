#pragma once
#include "intra-coord-fwd.hpp"
#include "global-coords.hpp"
#include "tile-defs.hpp"
#include "compat/assert.hpp"

namespace floormat {

struct point;

// Pixel position in a chunk, from its NW corner: [0, chunk_size) per axis.
// center_shifted() is the point frame, where the center of tile 0 is {0, 0}.
// Checking asserts every value it stores. Wrapping stores anything, and its += and -= wrap into the chunk.
template<intra_coord_base::Type TYPE>
struct basic_intra_coord : intra_coord_base
{
    constexpr basic_intra_coord() = default;
    explicit constexpr basic_intra_coord(Vector2i value);
    explicit constexpr basic_intra_coord(const point& pt);
    constexpr basic_intra_coord(local_coords local, Vector2b offset);
    template<Type OTHER> requires (OTHER != TYPE) constexpr basic_intra_coord(basic_intra_coord<OTHER> other);

    constexpr basic_intra_coord& operator=(Vector2i value);

    static constexpr bool is_in_range(Vector2i value);
    static constexpr basic_intra_coord from_center_shifted(Vector2i value);

    constexpr int x() const;
    constexpr int y() const;
    explicit constexpr operator Vector2i() const;
    explicit constexpr operator Vector2() const;

    constexpr local_coords local() const;
    constexpr Vector2b offset() const;
    constexpr Vector2i center_shifted() const;
    constexpr point to_point(chunk_coords_ ch) const;

    constexpr basic_intra_coord& operator+=(Vector2i delta) requires (TYPE == Checking);
    constexpr basic_intra_coord& operator-=(Vector2i delta) requires (TYPE == Checking);
    // returns the number of chunks crossed
    constexpr Vector2i operator+=(Vector2i delta) requires (TYPE == Wrapping);
    constexpr Vector2i operator-=(Vector2i delta) requires (TYPE == Wrapping);

    constexpr bool operator==(const basic_intra_coord&) const = default;

private:
    Vector2i _v;
};

template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Checking)
constexpr basic_intra_coord<TYPE> operator+(basic_intra_coord<TYPE> a, Vector2i b);
template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Checking)
constexpr basic_intra_coord<TYPE> operator+(Vector2i a, basic_intra_coord<TYPE> b);
template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Checking)
constexpr basic_intra_coord<TYPE> operator-(basic_intra_coord<TYPE> a, Vector2i b);

// the point's chunk is the number of chunks crossed, with z 0
template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Wrapping)
constexpr point operator+(basic_intra_coord<TYPE> a, Vector2i b);
template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Wrapping)
constexpr point operator+(Vector2i a, basic_intra_coord<TYPE> b);
template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Wrapping)
constexpr point operator-(basic_intra_coord<TYPE> a, Vector2i b);

template<intra_coord_base::Type TYPE>
constexpr Vector2i operator-(basic_intra_coord<TYPE> a, basic_intra_coord<TYPE> b);

template<intra_coord_base::Type TYPE>
Debug& operator<<(Debug& dbg, const basic_intra_coord<TYPE>& val);

template<intra_coord_base::Type TYPE>
constexpr bool basic_intra_coord<TYPE>::is_in_range(Vector2i value)
{
    return bool(value >= Vector2i{0} && value < chunk_size<Vector2i>);
}

template<intra_coord_base::Type TYPE>
constexpr basic_intra_coord<TYPE>::basic_intra_coord(Vector2i value) : _v{value}
{
    if constexpr (TYPE == Checking)
        fm_assert(is_in_range(value));
}

template<intra_coord_base::Type TYPE> constexpr int basic_intra_coord<TYPE>::x() const { return _v.x(); }
template<intra_coord_base::Type TYPE> constexpr int basic_intra_coord<TYPE>::y() const { return _v.y(); }
template<intra_coord_base::Type TYPE> constexpr basic_intra_coord<TYPE>::operator Vector2i() const { return _v; }
template<intra_coord_base::Type TYPE> constexpr basic_intra_coord<TYPE>::operator Vector2() const { return Vector2(_v); }

} // namespace floormat
