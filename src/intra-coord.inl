#pragma once
#include "intra-coord.hpp"
#include "point.hpp"
#include "compat/floor-divmod.hpp"

namespace floormat {

// point's constructors keep its offset in [-32, 32), so the value is in range.
template<intra_coord_base::Type TYPE>
constexpr basic_intra_coord<TYPE>::basic_intra_coord(const point& pt) :
    _v{Vector2i(basic_intra_coord<Wrapping>{pt.local(), pt.offset()})}
{
    fm_debug3_assert(is_in_range(_v));
}

template<intra_coord_base::Type TYPE>
constexpr basic_intra_coord<TYPE>::basic_intra_coord(local_coords local, Vector2b offset) :
    basic_intra_coord{Vector2i(local)*tile_size_xy + Vector2i(offset) + half_tile<Vector2i>}
{}

template<intra_coord_base::Type TYPE>
template<intra_coord_base::Type OTHER> requires (OTHER != TYPE)
constexpr basic_intra_coord<TYPE>::basic_intra_coord(basic_intra_coord<OTHER> other) : _v{Vector2i(other)}
{
    if constexpr (TYPE == Checking)
        fm_assert(is_in_range(_v));
}

template<intra_coord_base::Type TYPE>
constexpr basic_intra_coord<TYPE> basic_intra_coord<TYPE>::from_center_shifted(Vector2i value)
{
    return basic_intra_coord{value + half_tile<Vector2i>};
}

template<intra_coord_base::Type TYPE>
constexpr local_coords basic_intra_coord<TYPE>::local() const
{
    if constexpr (TYPE == Checking)
    {
        const int x = _v.x(), y = _v.y();
        [[assume(x >= 0 && x < chunk_size<int> && y >= 0 && y < chunk_size<int>)]];
    }
    return local_coords{_v / tile_size_xy};
}

template<intra_coord_base::Type TYPE>
constexpr Vector2b basic_intra_coord<TYPE>::offset() const { return Vector2b(_v % tile_size_xy - half_tile<Vector2i>); }

template<intra_coord_base::Type TYPE>
constexpr Vector2i basic_intra_coord<TYPE>::center_shifted() const { return _v - half_tile<Vector2i>; }

template<intra_coord_base::Type T> requires (T == intra_coord_base::Checking)
constexpr basic_intra_coord<T> operator+(basic_intra_coord<T> a, Vector2i b)
{
    return basic_intra_coord<T>{a._v + b};
}

template<intra_coord_base::Type T> requires (T == intra_coord_base::Checking)
constexpr basic_intra_coord<T> operator+(Vector2i a, basic_intra_coord<T> b)
{
    return b + a;
}

template<intra_coord_base::Type T> requires (T == intra_coord_base::Checking)
constexpr basic_intra_coord<T> operator-(basic_intra_coord<T> a, Vector2i b)
{
    return basic_intra_coord<T>{a._v - b};
}

template<intra_coord_base::Type T> requires (T == intra_coord_base::Wrapping)
constexpr point operator+(basic_intra_coord<T> a, Vector2i b)
{
    const auto [chunks, pos] = floor_divmod<chunk_size<int>>(a._v + b);
    return point{chunk_coords_{(int16_t)chunks.x(), (int16_t)chunks.y(), 0}, intra_coord{pos}};
}

template<intra_coord_base::Type T> requires (T == intra_coord_base::Wrapping)
constexpr point operator+(Vector2i a, basic_intra_coord<T> b)
{
    return b + a;
}

template<intra_coord_base::Type T> requires (T == intra_coord_base::Wrapping)
constexpr point operator-(basic_intra_coord<T> a, Vector2i b)
{
    return a + -b;
}

template<intra_coord_base::Type T>
constexpr Vector2i operator-(basic_intra_coord<T> a, basic_intra_coord<T> b)
{
    return a._v - b._v;
}

constexpr point::point(chunk_coords_ coord, intra_coord ic) : point{coord, ic.local(), ic.offset()} {}

template<intra_coord_base::Type TYPE>
constexpr basic_intra_coord<TYPE> point::intra() const { return basic_intra_coord<TYPE>{*this}; }

} // namespace floormat
