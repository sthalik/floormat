#pragma once
#include "local-coords.hpp"
#include <new>
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
        fm_assert(z >= chunk_z_min && z <= chunk_z_max);
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
        ::new (static_cast<void*>(this)) auto{other};
    return *this;
}

struct global_coords final
{
    struct raw_coords_ { u32 x, y; operator Vector2ui() const; };

private:
    using u0 = std::integral_constant<u32, chunk_xy_bias>;
    using s0 = std::integral_constant<i32, i32(u0::value)>;
    using z0 = std::integral_constant<i32, (1 << 0)>;
    using z_mask = std::integral_constant<u32, (1u << 4) - 1u << 20>;
    const u32 x = u0::value<<4|z0::value<<20, y = u0::value<<4;

public:
    constexpr global_coords() noexcept = default;
    constexpr global_coords(const global_coords&) noexcept = default;
    constexpr global_coords& operator=(const global_coords& other) noexcept;
    constexpr global_coords(chunk_coords c, local_coords xy, i8 z) noexcept :
        global_coords{chunk_coords_{c, z}, xy}
    {}
    constexpr global_coords(u32 x, u32 y, std::nullptr_t) noexcept : x{x}, y{y}
    {
        i32 cx = i32((x & ~z_mask::value) >> 4) - s0::value, cy = i32(y >> 4) - s0::value;
        fm_assert(cx >= chunk_xy_min && cx <= chunk_xy_max && cy >= chunk_xy_min && cy <= chunk_xy_max);
    }
    constexpr global_coords(u32, u32, u32) = delete;
    constexpr global_coords(i32 x, i32 y, i8 z) noexcept :
        x{u32(x + (s0::value<<4)) | u32(((z + z0::value) & 0x0f) << 20)},
        y{u32(y + (s0::value<<4))}
    {
        fm_assert((x>>4) >= chunk_xy_min && (x>>4) <= chunk_xy_max && (y>>4) >= chunk_xy_min && (y>>4) <= chunk_xy_max
               && z >= chunk_z_min && z <= chunk_z_max);
    }
    constexpr global_coords(chunk_coords_ c, local_coords xy) noexcept :
        x{
            u32((c.x + s0::value) << 4) | (xy.x & 0x0f) |
            u32(((int)c.z + z0::value) & 0x0f) << 20
        },
        y{ u32((c.y + s0::value) << 4) | (xy.y & 0x0f) }
    {}

    constexpr local_coords local() const noexcept;
    constexpr chunk_coords chunk() const noexcept;
    constexpr chunk_coords_ chunk3() const noexcept { return chunk_coords_(*this); }
    constexpr operator chunk_coords_() const noexcept;
    constexpr raw_coords_ raw() const noexcept;
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
};

constexpr global_coords& global_coords::operator=(const global_coords& other) noexcept
{
    if (this != &other)
        ::new (static_cast<void*>(this)) auto{other};
    return *this;
}

constexpr local_coords global_coords::local() const noexcept
{
    return { u8(x & 0x0f), u8(y & 0x0f), };
}

constexpr chunk_coords global_coords::chunk() const noexcept
{
    return { i16(i32(((x & ~z_mask::value)>>4) - u0::value)), i16(i32((y>>4) - u0::value)), };
}

constexpr global_coords::operator chunk_coords_() const noexcept
{
    return chunk_coords_{ chunk(), z() };
}

constexpr auto global_coords::raw() const noexcept -> raw_coords_ { return {x, y}; }

constexpr i8 global_coords::z() const noexcept
{
    return ((x >> 20) & 0x0f) - z0::value;
}

template<typename T> constexpr global_coords::operator Math::Vector2<T>() const noexcept
{
    static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(i32));
    return { (T)i32((x & ~z_mask::value) - (s0::value<<4)), (T)i32(y - (s0::value<<4)), };
}

template<typename T> constexpr global_coords::operator Math::Vector3<T>() const noexcept
{
    static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(i32));
    return Math::Vector3<T>(Math::Vector2<T>(*this), (T)z());
}

constexpr global_coords global_coords::operator+(Vector2i vec) const noexcept
{
    const auto nx = u32((i64)x+vec[0]);
    // a carry out of chunk x lands in the z bits, which the constructor's range check masks off
    fm_assert(((nx ^ x) & z_mask::value) == 0);
    return { nx, u32((i64)y+vec[1]), nullptr };
}

constexpr global_coords& global_coords::operator+=(Vector2i vec) noexcept
{
    return *this = *this + vec;
}

constexpr global_coords global_coords::operator-(Vector2i vec) const noexcept
{
    const auto nx = u32((i64)x-vec[0]);
    fm_assert(((nx ^ x) & z_mask::value) == 0);
    return { nx, u32((i64)y-vec[1]), nullptr };
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
