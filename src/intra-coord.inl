#pragma once
#include "intra-coord.hpp"
#include "point.hpp"
#include "compat/floor-divmod.hpp"

namespace floormat {

template<intra_coord_base::Type TYPE>
constexpr basic_intra_coord<TYPE>::basic_intra_coord(const point& pt) : basic_intra_coord{pt.local(), pt.offset()} {}

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
constexpr basic_intra_coord<TYPE>& basic_intra_coord<TYPE>::operator=(Vector2i value)
{
    if constexpr (TYPE == Checking)
        fm_assert(is_in_range(value));
    _v = value;
    return *this;
}

template<intra_coord_base::Type TYPE>
constexpr basic_intra_coord<TYPE> basic_intra_coord<TYPE>::from_center_shifted(Vector2i value)
{
    return basic_intra_coord{value + half_tile<Vector2i>};
}

template<intra_coord_base::Type TYPE>
constexpr local_coords basic_intra_coord<TYPE>::local() const { return local_coords{_v / tile_size_xy}; }

template<intra_coord_base::Type TYPE>
constexpr Vector2b basic_intra_coord<TYPE>::offset() const { return Vector2b(_v % tile_size_xy - half_tile<Vector2i>); }

template<intra_coord_base::Type TYPE>
constexpr Vector2i basic_intra_coord<TYPE>::center_shifted() const { return _v - half_tile<Vector2i>; }

template<intra_coord_base::Type TYPE>
constexpr auto basic_intra_coord<TYPE>::to_point(chunk_coords_ ch) const -> point
{
    return point{ch, local(), offset()};
}

template<intra_coord_base::Type TYPE>
constexpr basic_intra_coord<TYPE>& basic_intra_coord<TYPE>::operator+=(Vector2i delta) requires (TYPE == Checking)
{
    _v += delta;
    fm_assert(is_in_range(_v));
    return *this;
}

template<intra_coord_base::Type TYPE>
constexpr basic_intra_coord<TYPE>& basic_intra_coord<TYPE>::operator-=(Vector2i delta) requires (TYPE == Checking)
{
    return *this += -delta;
}

template<intra_coord_base::Type TYPE>
constexpr Vector2i basic_intra_coord<TYPE>::operator+=(Vector2i delta) requires (TYPE == Wrapping)
{
    const auto [chunks, pos] = floor_divmod<chunk_size<int>>(_v + delta);
    _v = pos;
    return chunks;
}

template<intra_coord_base::Type TYPE>
constexpr Vector2i basic_intra_coord<TYPE>::operator-=(Vector2i delta) requires (TYPE == Wrapping)
{
    return *this += -delta;
}

template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Checking)
constexpr basic_intra_coord<TYPE> operator+(basic_intra_coord<TYPE> a, Vector2i b)
{
    a += b;
    return a;
}

template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Checking)
constexpr basic_intra_coord<TYPE> operator+(Vector2i a, basic_intra_coord<TYPE> b)
{
    return b + a;
}

template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Checking)
constexpr basic_intra_coord<TYPE> operator-(basic_intra_coord<TYPE> a, Vector2i b)
{
    a -= b;
    return a;
}

template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Wrapping)
constexpr point operator+(basic_intra_coord<TYPE> a, Vector2i b)
{
    const auto chunks = (a += b);
    return a.to_point({(int16_t)chunks.x(), (int16_t)chunks.y(), 0});
}

template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Wrapping)
constexpr point operator+(Vector2i a, basic_intra_coord<TYPE> b)
{
    return b + a;
}

template<intra_coord_base::Type TYPE> requires (TYPE == intra_coord_base::Wrapping)
constexpr point operator-(basic_intra_coord<TYPE> a, Vector2i b)
{
    return a + -b;
}

template<intra_coord_base::Type TYPE>
constexpr Vector2i operator-(basic_intra_coord<TYPE> a, basic_intra_coord<TYPE> b)
{
    return Vector2i(a) - Vector2i(b);
}

constexpr point::point(chunk_coords_ coord, intra_coord ic) : point{coord, ic.local(), ic.offset()} {}

template<intra_coord_base::Type TYPE>
constexpr basic_intra_coord<TYPE> point::intra() const { return basic_intra_coord<TYPE>{tile, _offset}; }

} // namespace floormat
