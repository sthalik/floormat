#pragma once
#include "point.hpp"
#include "tile-constants.hpp"
#include "compat/floor-divmod.hpp"
#include <mg/Functions.h>

namespace floormat {

constexpr point::point(Vector3i p)
{
    const auto z = floor_div<tile_size_z>(p.z());
    // a pixel out of range would wrap to another position, so assert before packing
    fm_assert(p.x() >= most_negative_point_xy && p.x() <= most_positive_point_xy
           && p.y() >= most_negative_point_xy && p.y() <= most_positive_point_xy
           && z >= chunk_z_min && z <= chunk_z_max);
    _bits = global_coords::pack_point(p);
}

constexpr u32 point::distance(point a, point b)
{
    Vector2i dist = a - b;
    return (u32)Math::ceil(Vector2(dist).length());
}

constexpr u64 point::distance_squared(Vector2i delta)
{
    return (u64)((i64)delta.x()*delta.x() + (i64)delta.y()*delta.y());
}

constexpr u64 point::distance_squared(point a, point b) { return distance_squared(a - b); }

constexpr u32 point::distance_l1(point a, point b)
{
    Vector2i dist = a - b;
    return (u32)Math::abs(dist).sum();
}

constexpr std::strong_ordering operator<=>(const point& p1, const point& p2)
{
    // z, then chunk y, chunk x, tile y, tile x, offset y and offset x
    constexpr auto key = [](u64 bits) {
        using G = global_coords;
        constexpr u32 offset_bits = std::countr_zero(u32{tile_size_xy}), tile_bits = std::countr_zero(TILE_MAX_DIM);
        const u64 x = (bits & G::point_bitmasks[0]) >> G::point_shift_counts[0];
        const u64 y = (bits & G::point_bitmasks[1]) >> G::point_shift_counts[1];
        return (bits & G::point_bitmasks[2])
             | (y / chunk_size_xy) << (chunk_coord_bits + 2*tile_bits + 2*offset_bits)
             | (x / chunk_size_xy) << (2*tile_bits + 2*offset_bits)
             | (y / u32{tile_size_xy} % TILE_MAX_DIM) << (tile_bits + 2*offset_bits)
             | (x / u32{tile_size_xy} % TILE_MAX_DIM) << 2*offset_bits
             | (y % u32{tile_size_xy}) << offset_bits | x % u32{tile_size_xy};
    };
    return key(p1._bits) <=> key(p2._bits);
}

constexpr Vector2i operator-(const point& p1, const point& p2)
{
    fm_debug3_assert(p1.chunk3().z == p2.chunk3().z);
    const auto d = Vector3i(p1) - Vector3i(p2);
    return d.xy();
}

constexpr point::operator Vector3i() const
{
    return global_coords::unpack_point(_bits);
}

constexpr point::operator Vector3() const { return Vector3{Vector3i(*this)}; }

} // namespace floormat
