#pragma once
#include "global-coords.hpp"
#include "intra-coord-fwd.hpp"
#include <compare>

namespace floormat {

struct point
{
    constexpr point();
    constexpr point(global_coords coord, Vector2b offset);
    constexpr point(chunk_coords_ coord, local_coords tile, Vector2b offset);
    constexpr point(chunk_coords_ coord, intra_coord ic);
    explicit constexpr point(Vector3i pixel);

    constexpr bool operator==(const point&) const noexcept;
    friend constexpr std::strong_ordering operator<=>(const point& a, const point& b);
    friend constexpr Vector2i operator-(const point& p1, const point& p2);
    constexpr explicit operator Vector3i() const;
    constexpr explicit operator Vector3() const;

    constexpr global_coords coord() const;
    constexpr chunk_coords chunk() const;
    constexpr chunk_coords_ chunk3() const;
    constexpr local_coords local() const;
    constexpr Vector2b offset() const;
    template<intra_coord_base::Type TYPE = intra_coord_base::Checking> constexpr basic_intra_coord<TYPE> intra() const;

    friend Debug& operator<<(Debug& dbg, const point& pt);

    static constexpr u32 distance(point a, point b);
    static constexpr u64 distance_squared(Vector2i delta);
    static constexpr u64 distance_squared(point a, point b);
    static constexpr u32 distance_l1(point a, point b);

    static point normalize_coords(global_coords coord, Vector2b cur_offset, Vector2i delta);
    static point normalize_coords(point pt, Vector2i delta);

    friend point operator+(point pt, Vector2i delta);
    friend point operator+(Vector2i delta, point pt);

    friend point operator-(point pt, Vector2i delta);
    friend point operator-(Vector2i delta, point pt);

private:
    u64 _bits = global_coords::pack_point({});
};

static_assert(sizeof(point) == 8);

constexpr point::point() = default;
constexpr point::point(global_coords coord, Vector2b offset) :
    _bits{global_coords::pack_point(global_coords::unpack_point(coord._bits) + Vector3i{offset.x(), offset.y(), 0})}
{
    fm_assert(offset >= -half_tile<Vector2b> && offset < half_tile<Vector2b>);
}
constexpr point::point(chunk_coords_ coord, local_coords tile, Vector2b offset) :
    point{global_coords{coord, tile}, offset}
{}

constexpr bool point::operator==(const point&) const noexcept = default;

constexpr global_coords point::coord() const
{
    using G = global_coords;
    const u64 z = (_bits & G::point_bitmasks[2]) >> G::point_shift_counts[2];
    u64 bits = (_bits & ~G::point_bitmasks[2])
             | (((z - z % u32{tile_size_z}) << G::point_shift_counts[2]) & G::point_bitmasks[2]);
    for (u32 i = 0; i < 2; i++)
        bits = (bits & ~(u64{tile_size_xy - 1} << G::point_shift_counts[i]))
             | (u64{half_tile<i32>} << G::point_shift_counts[i]);
    return { bits, G::Unchecked{} };
}

constexpr chunk_coords_ point::chunk3() const { return global_coords{_bits, global_coords::Unchecked{}}.chunk3(); }
constexpr chunk_coords point::chunk() const { return global_coords{_bits, global_coords::Unchecked{}}.chunk(); }
constexpr local_coords point::local() const { return global_coords{_bits, global_coords::Unchecked{}}.local(); }

constexpr Vector2b point::offset() const
{
    using G = global_coords;
    const auto x = u32((_bits & G::point_bitmasks[0]) >> G::point_shift_counts[0]);
    const auto y = u32((_bits & G::point_bitmasks[1]) >> G::point_shift_counts[1]);
    return { i8(i32(x % u32{tile_size_xy}) - half_tile<i32>), i8(i32(y % u32{tile_size_xy}) - half_tile<i32>), };
}

} // namespace floormat
