#pragma once
#include "local-coords.hpp"
#include "compat/construct-at.hpp"
#include "compat/floor-divmod.hpp"
#include <bit>
#include <mg/Vector3.h>

namespace floormat {

struct chunk_coords final {
    i16 x = 0, y = 0;

    constexpr bool operator==(const chunk_coords& other) const noexcept = default;
    friend Debug& operator<<(Debug& dbg, const chunk_coords& pt);

    template<typename T>
    explicit constexpr operator Math::Vector2<T>() const noexcept
    {
        static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(i16));
        return Math::Vector2<T>(T(x), T(y));
    }

    template<typename T>
    explicit constexpr inline operator Math::Vector3<T>() const noexcept
    {
        static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(i16));
        return Math::Vector3<T>(T(x), T(y), T(0));
    }

    constexpr Vector2i operator-(chunk_coords other) const noexcept { return Vector2i{x - other.x, y - other.y }; }
};

struct chunk_coords_ final {
    const i16 x = 0, y = 0;
    const i8 z = 0;

    constexpr chunk_coords_() noexcept = default;
    constexpr chunk_coords_(i16 x, i16 y, i8 z) noexcept : x{x}, y{y}, z{z}
    {
        // Plain comparisons trip -Wtautological-type-limit-compare while chunk_coord_bits is 16.
        fm_assert(u32(x - chunk_xy_min) <= u32(chunk_xy_max - chunk_xy_min)
                  && u32(y - chunk_xy_min) <= u32(chunk_xy_max - chunk_xy_min) && z >= chunk_z_min && z <= chunk_z_max);
    }
    constexpr chunk_coords_(chunk_coords c, i8 z) noexcept : chunk_coords_{c.x, c.y, z} {}
    constexpr chunk_coords_(const chunk_coords_&) noexcept = default;
    constexpr chunk_coords_& operator=(const chunk_coords_& other) noexcept;

    constexpr bool operator==(const chunk_coords_&) const noexcept = default;
    friend Debug& operator<<(Debug& dbg, const chunk_coords_& pt);

    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_ operator+(Math::Vector2<T> off) const noexcept
    {
        [[assume(z >= chunk_z_min && z <= chunk_z_max)]];
        return { i16(x + int{off.x()}), i16(y + int{off.y()}), z };
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_ operator-(Math::Vector2<T> off) const noexcept
    {
        [[assume(z >= chunk_z_min && z <= chunk_z_max)]];
        return { i16(x - int{off.x()}), i16(y - int{off.y()}), z };
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_& operator+=(Math::Vector2<T> off) noexcept
    {
        return *this = *this + off;
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_& operator-=(Math::Vector2<T> off) noexcept
    {
        return *this = *this - off;
    }

    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_ operator+(Math::Vector3<T> off) const noexcept
    {
        return { i16(x + int{off.x()}), i16(y + int{off.y()}), i8(z + int{off.z()}) };
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_ operator-(Math::Vector3<T> off) const noexcept
    {
        return { i16(x - int{off.x()}), i16(y - int{off.y()}), i8(z - int{off.z()}) };
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_& operator+=(Math::Vector3<T> off) noexcept
    {
        return *this = *this + off;
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_& operator-=(Math::Vector3<T> off) noexcept
    {
        return *this = *this - off;
    }

    constexpr Vector3i operator-(chunk_coords_ other) const noexcept
    {
        return Vector3i{x - other.x, y - other.y, z - other.z};
    }

    explicit constexpr inline operator chunk_coords() const noexcept { return chunk_coords{x, y}; }

    template<typename T> explicit constexpr inline operator Math::Vector3<T>() const noexcept
    {
        static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(i16));
        return Math::Vector3<T>(x, y, z);
    }

    usize hash() const noexcept;
};

constexpr chunk_coords_& chunk_coords_::operator=(const chunk_coords_& other) noexcept
{
    if (this != &other)
        construct_at(this, other);
    return *this;
}

struct global_coords final
{
    // Each coordinate is stored plus its bias. An x or y holds chunk - chunk_xy_min, tile and offset + half_tile,
    // highest bits first. global_coords holds offset 0 and whole levels of z.
    static constexpr Math::Vector3<u64> point_shift_counts{
        0, chunk_coord_bits + std::countr_zero(chunk_size_xy), 2*(chunk_coord_bits + std::countr_zero(chunk_size_xy)),
    };
    static constexpr Math::Vector3<u64> point_bitmasks{
        (u64{1} << point_shift_counts[1]) - 1,
        (u64{1} << point_shift_counts[2]) - (u64{1} << point_shift_counts[1]),
        ~u64{0} << point_shift_counts[2],
    };
    static constexpr Vector3i point_biases{-most_negative_point_xy, -most_negative_point_xy, -most_negative_point_z};

    constexpr global_coords() noexcept = default;
    constexpr global_coords(const global_coords&) noexcept = default;
    constexpr global_coords& operator=(const global_coords& other) noexcept;
    constexpr global_coords(chunk_coords c, local_coords xy, i8 z) noexcept :
        global_coords{chunk_coords_{c, z}, xy}
    {}
    constexpr global_coords(u32, u32, u32) = delete;
    constexpr global_coords(i32 x, i32 y, i8 z) noexcept :
        _bits{pack_point({x*tile_size_xy, y*tile_size_xy, z*tile_size_z})}
    {
        constexpr i32 min_tile = chunk_xy_min * i32{TILE_MAX_DIM}, end_tile = (chunk_xy_max + 1) * i32{TILE_MAX_DIM};
        fm_assert(x >= min_tile && x < end_tile && y >= min_tile && y < end_tile && z >= chunk_z_min && z <= chunk_z_max);
    }
    constexpr global_coords(chunk_coords_ c, local_coords xy) noexcept :
        _bits{pack_point({(c.x*i32{TILE_MAX_DIM} + xy.x)*tile_size_xy, (c.y*i32{TILE_MAX_DIM} + xy.y)*tile_size_xy,
                          c.z*tile_size_z})}
    {
        [[assume(c.z >= chunk_z_min && c.z <= chunk_z_max)]];
    }

    constexpr local_coords local() const noexcept;
    constexpr chunk_coords chunk() const noexcept;
    constexpr chunk_coords_ chunk3() const noexcept { return chunk_coords_(*this); }
    constexpr operator chunk_coords_() const noexcept;
    constexpr i8 z() const noexcept;

    template<typename T> explicit constexpr inline operator Math::Vector2<T>() const noexcept;
    template<typename T> explicit constexpr inline operator Math::Vector3<T>() const noexcept;
    constexpr bool operator==(const global_coords& other) const noexcept = default;

    constexpr global_coords operator+(Vector2i vec) const noexcept;
    constexpr global_coords operator-(Vector2i vec) const noexcept;
    constexpr global_coords& operator+=(Vector2i vec) noexcept;
    constexpr global_coords& operator-=(Vector2i vec) noexcept;
    constexpr Vector2i operator-(global_coords other) const noexcept;

    usize hash() const noexcept;

private:
    friend struct point;
    struct Unchecked {};
    constexpr global_coords(u64 bits, Unchecked) noexcept : _bits{bits} {}

    static constexpr u64 pack_point(Vector3i px) noexcept
    {
        u64 bits = 0;
        for (u32 i = 0; i < 3; i++)
            bits |= (u64(u32(px[i] + point_biases[i])) << point_shift_counts[i]) & point_bitmasks[i];
        return bits;
    }
    static constexpr Vector3i unpack_point(u64 bits) noexcept
    {
        Vector3i px;
        for (u32 i = 0; i < 3; i++)
            px[i] = i32((bits & point_bitmasks[i]) >> point_shift_counts[i]) - point_biases[i];
        return px;
    }

    const u64 _bits = pack_point({});
};

constexpr global_coords& global_coords::operator=(const global_coords& other) noexcept
{
    if (this != &other)
        construct_at(this, other);
    return *this;
}

constexpr local_coords global_coords::local() const noexcept
{
    const auto x = u32((_bits & point_bitmasks[0]) >> point_shift_counts[0]);
    const auto y = u32((_bits & point_bitmasks[1]) >> point_shift_counts[1]);
    return { u8(x / u32{tile_size_xy} % TILE_MAX_DIM), u8(y / u32{tile_size_xy} % TILE_MAX_DIM), };
}

constexpr chunk_coords global_coords::chunk() const noexcept
{
    const auto x = u32((_bits & point_bitmasks[0]) >> point_shift_counts[0]);
    const auto y = u32((_bits & point_bitmasks[1]) >> point_shift_counts[1]);
    return { i16(i32(x / chunk_size_xy) + chunk_xy_min), i16(i32(y / chunk_size_xy) + chunk_xy_min), };
}

constexpr global_coords::operator chunk_coords_() const noexcept
{
    return chunk_coords_{ chunk(), z() };
}

constexpr i8 global_coords::z() const noexcept
{
    const auto z = u32((_bits & point_bitmasks[2]) >> point_shift_counts[2]);
    [[assume(z < u32(chunk_z_count * tile_size_z))]];
    return i8(i32(z / u32{tile_size_z}) + chunk_z_min);
}

template<typename T> constexpr global_coords::operator Math::Vector2<T>() const noexcept
{
    static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(i32));
    const auto x = u32((_bits & point_bitmasks[0]) >> point_shift_counts[0]);
    const auto y = u32((_bits & point_bitmasks[1]) >> point_shift_counts[1]);
    return { T(i32(x / u32{tile_size_xy}) + chunk_xy_min * i32{TILE_MAX_DIM}),
             T(i32(y / u32{tile_size_xy}) + chunk_xy_min * i32{TILE_MAX_DIM}), };
}

template<typename T> constexpr global_coords::operator Math::Vector3<T>() const noexcept
{
    static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(i32));
    return Math::Vector3<T>(Math::Vector2<T>(*this), (T)z());
}

constexpr global_coords global_coords::operator+(Vector2i vec) const noexcept
{
    u64 bits = _bits & point_bitmasks[2];
    for (u32 i = 0; i < 2; i++)
    {
        const auto v = u64(i64((_bits & point_bitmasks[i]) >> point_shift_counts[i]) + i64{vec[i]} * tile_size_xy);
        fm_assert(v <= point_bitmasks[i] >> point_shift_counts[i]);
        bits |= (v << point_shift_counts[i]) & point_bitmasks[i];
    }
    return { bits, Unchecked{} };
}

constexpr global_coords& global_coords::operator+=(Vector2i vec) noexcept
{
    return *this = *this + vec;
}

constexpr global_coords global_coords::operator-(Vector2i vec) const noexcept
{
    return *this + -vec;
}

constexpr global_coords& global_coords::operator-=(Vector2i vec) noexcept
{
    return *this = *this - vec;
}

constexpr Vector2i global_coords::operator-(global_coords other) const noexcept
{
    return Vector2i(*this) - Vector2i(other);
}

namespace Hash { struct chunk_coord_hasher { static usize operator()(chunk_coords_ coord) noexcept; }; }

} // namespace floormat
