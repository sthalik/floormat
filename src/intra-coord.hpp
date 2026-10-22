#pragma once
#include "intra-coord-fwd.hpp"
#include "global-coords.hpp"
#include "tile-defs.hpp"
#include "compat/assert.hpp"

namespace floormat {

struct point;

// Pixel position in a chunk, from its NW corner: [0, chunk_size) per axis.
// Checking asserts every value it stores. Wrapping stores anything, and its + and - wrap into the chunk.
template<intra_coord_base::Type TYPE>
struct basic_intra_coord : intra_coord_base
{
    constexpr basic_intra_coord() = default;
    explicit constexpr basic_intra_coord(Vector2i value);
    explicit constexpr basic_intra_coord(const point& pt);
    constexpr basic_intra_coord(local_coords local, Vector2b offset);
    template<Type OTHER> requires (OTHER != TYPE) constexpr basic_intra_coord(basic_intra_coord<OTHER> other);

    static constexpr bool is_in_range(Vector2i value);
    /// @brief Inverse of center_shifted().
    /// @param value Position where the center of the first tile is {0, 0}.
    static constexpr basic_intra_coord from_center_shifted(Vector2i value);

    constexpr int x() const;
    constexpr int y() const;
    explicit constexpr operator Vector2i() const;
    explicit constexpr operator Vector2() const;

    constexpr local_coords local() const;
    constexpr Vector2b offset() const;
    /// @brief Moves the origin from the chunk's top-left corner to the center of the first tile, as in `point`.
    /// @return Position where the center of the first tile is {0, 0}.
    constexpr Vector2i center_shifted() const;

    template<intra_coord_base::Type T> requires (T == intra_coord_base::Checking)
    friend constexpr basic_intra_coord<T> operator+(basic_intra_coord<T> a, Vector2i b);
    template<intra_coord_base::Type T> requires (T == intra_coord_base::Checking)
    friend constexpr basic_intra_coord<T> operator+(Vector2i a, basic_intra_coord<T> b);
    template<intra_coord_base::Type T> requires (T == intra_coord_base::Checking)
    friend constexpr basic_intra_coord<T> operator-(basic_intra_coord<T> a, Vector2i b);

    /// @brief Adds @p b and wraps the position into the chunk.
    /// @return The point in chunk {x, y, 0}, where x and y count the chunks crossed: -1 after going below 0.
    template<intra_coord_base::Type T> requires (T == intra_coord_base::Wrapping)
    friend constexpr point operator+(basic_intra_coord<T> a, Vector2i b);
    template<intra_coord_base::Type T> requires (T == intra_coord_base::Wrapping)
    friend constexpr point operator+(Vector2i a, basic_intra_coord<T> b);
    template<intra_coord_base::Type T> requires (T == intra_coord_base::Wrapping)
    friend constexpr point operator-(basic_intra_coord<T> a, Vector2i b);

    template<intra_coord_base::Type T>
    friend constexpr Vector2i operator-(basic_intra_coord<T> a, basic_intra_coord<T> b);

    template<intra_coord_base::Type T>
    friend Debug& operator<<(Debug& dbg, const basic_intra_coord<T>& val);

    constexpr bool operator==(const basic_intra_coord&) const = default;

private:
    Vector2i _v;
};

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
