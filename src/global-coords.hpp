#pragma once
#include "local-coords.hpp"
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

namespace floormat {

struct chunk_coords final {
    int16_t x = 0, y = 0;

    constexpr bool operator==(const chunk_coords& other) const noexcept = default;
    friend Debug& operator<<(Debug& dbg, const chunk_coords& pt);

    template<typename T>
    explicit constexpr operator Math::Vector2<T>() const noexcept
    {
        static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(int16_t));
        return Math::Vector2<T>(T(x), T(y));
    }

    template<typename T>
    explicit constexpr inline operator Math::Vector3<T>() const noexcept
    {
        static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(int16_t));
        return Math::Vector3<T>(T(x), T(y), T(0));
    }

    constexpr Vector2i operator-(chunk_coords other) const noexcept { return Vector2i{x - other.x, y - other.y }; }
};

struct chunk_coords_ final {
    int16_t x = 0, y = 0;
    int8_t z = 0;

    constexpr chunk_coords_() noexcept = default;
    constexpr chunk_coords_(int16_t x, int16_t y, int8_t z) noexcept : x{x}, y{y}, z{z} {}
    constexpr chunk_coords_(chunk_coords c, int8_t z) noexcept : x{c.x}, y{c.y}, z{z} {}

    constexpr bool operator==(const chunk_coords_&) const noexcept = default;
    friend Debug& operator<<(Debug& dbg, const chunk_coords_& pt);

    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_ operator+(Math::Vector2<T> off) const noexcept
    {
        return { int16_t(x + int{off.x()}), int16_t(y + int{off.y()}), z };
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_ operator-(Math::Vector2<T> off) const noexcept
    {
        return { int16_t(x - int{off.x()}), int16_t(y - int{off.y()}), z };
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_& operator+=(Math::Vector2<T> off) noexcept
    {
        x = int16_t(x + int{off.x()}); y = int16_t(y + int{off.y()}); return *this;
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_& operator-=(Math::Vector2<T> off) noexcept
    {
        x = int16_t(x - int{off.x()}); y = int16_t(y - int{off.y()}); return *this;
    }

    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_ operator+(Math::Vector3<T> off) const noexcept
    {
        return { int16_t(x + int{off.x()}), int16_t(y + int{off.y()}), int8_t(z + int{off.z()}) };
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_ operator-(Math::Vector3<T> off) const noexcept
    {
        return { int16_t(x - int{off.x()}), int16_t(y - int{off.y()}), int8_t(z - int{off.z()}) };
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_& operator+=(Math::Vector3<T> off) noexcept
    {
        x = int16_t(x + int{off.x()}); y = int16_t(y + int{off.y()}); z = int8_t(z + int{off.z()}); return *this;
    }
    template<typename T> requires std::is_integral_v<T> constexpr chunk_coords_& operator-=(Math::Vector3<T> off) noexcept
    {
        x = int16_t(x - int{off.x()}); y = int16_t(y - int{off.y()}); z = int8_t(z + int{off.z()}); return *this;
    }

    constexpr Vector3i operator-(chunk_coords_ other) const noexcept
    {
        return Vector3i{x - other.x, y - other.y, z - other.z};
    }

    explicit constexpr inline operator chunk_coords() const noexcept { return chunk_coords{x, y}; }

    template<typename T> explicit constexpr inline operator Math::Vector3<T>() const noexcept
    {
        static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(int16_t));
        return Math::Vector3<T>(x, y, z);
    }
};

constexpr inline int8_t chunk_z_min = -1, chunk_z_max = 14;

struct global_coords final
{
    struct raw_coords { uint32_t &x, &y; operator Vector2ui() const { return {x, y}; } }; // NOLINT
    struct raw_coords_ { uint32_t x, y; operator Vector2ui() const { return {x, y}; } };

private:
    using u0 = std::integral_constant<uint32_t, (1<<15)>;
    using s0 = std::integral_constant<int32_t, int32_t(u0::value)>;
    using z0 = std::integral_constant<int32_t, (1 << 0)>;
    using z_mask = std::integral_constant<uint32_t, (1u << 4) - 1u << 20>;
    uint32_t x = u0::value<<4|z0::value<<20, y = u0::value<<4;

public:
    constexpr global_coords() noexcept = default;
    constexpr global_coords(chunk_coords c, local_coords xy, int8_t z) noexcept :
        x{
            uint32_t((c.x + s0::value) << 4) | (xy.x & 0x0f) |
            uint32_t(((int)z + z0::value) & 0x0f) << 20
        },
        y{ uint32_t((c.y + s0::value) << 4) | (xy.y & 0x0f) }
    {}
    constexpr global_coords(uint32_t x, uint32_t y, std::nullptr_t) noexcept : x{x}, y{y} {}
    constexpr global_coords(uint32_t, uint32_t, uint32_t) = delete;
    constexpr global_coords(int32_t x, int32_t y, int8_t z) noexcept :
        x{uint32_t(x + (s0::value<<4)) | uint32_t(((z + z0::value) & 0x0f) << 20)},
        y{uint32_t(y + (s0::value<<4))}
    {}
    constexpr global_coords(chunk_coords_ c, local_coords xy) noexcept :
        global_coords{chunk_coords{c.x, c.y}, xy, c.z}
    {}

    constexpr local_coords local() const noexcept;
    constexpr chunk_coords chunk() const noexcept;
    constexpr chunk_coords_ chunk3() const noexcept { return chunk_coords_(*this); }
    constexpr operator chunk_coords_() const noexcept;
    constexpr raw_coords_ raw() const noexcept;
    constexpr raw_coords raw() noexcept;
    constexpr int8_t z() const noexcept;

    template<typename T> explicit constexpr inline operator Math::Vector2<T>() const noexcept;
    template<typename T> explicit constexpr inline operator Math::Vector3<T>() const noexcept;
    constexpr bool operator==(const global_coords& other) const noexcept = default;

    constexpr global_coords operator+(Vector2i vec) const noexcept;
    constexpr global_coords operator-(Vector2i vec) const noexcept;
    constexpr global_coords& operator+=(Vector2i vec) noexcept;
    constexpr global_coords& operator-=(Vector2i vec) noexcept;
    constexpr Vector2i operator-(global_coords other) const noexcept;

    size_t hash() const noexcept;
};

constexpr local_coords global_coords::local() const noexcept
{
    return { uint8_t(x & 0x0f), uint8_t(y & 0x0f), };
}

constexpr chunk_coords global_coords::chunk() const noexcept
{
    return { int16_t(int32_t(((x & ~z_mask::value)>>4) - u0::value)), int16_t(int32_t((y>>4) - u0::value)), };
}

constexpr global_coords::operator chunk_coords_() const noexcept
{
    return chunk_coords_{ chunk(), z() };
}

constexpr auto global_coords::raw() const noexcept -> raw_coords_ { return {x, y}; }
constexpr auto global_coords::raw() noexcept -> raw_coords { return {x, y}; }

constexpr int8_t global_coords::z() const noexcept
{
    return ((x >> 20) & 0x0f) - z0::value;
}

template<typename T> constexpr global_coords::operator Math::Vector2<T>() const noexcept
{
    static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(int32_t));
    return { (T)int32_t((x & ~z_mask::value) - (s0::value<<4)), (T)int32_t(y - (s0::value<<4)), };
}

template<typename T> constexpr global_coords::operator Math::Vector3<T>() const noexcept
{
    static_assert(std::is_signed_v<T> && sizeof(T) >= sizeof(int32_t));
    return Math::Vector3<T>(Math::Vector2<T>(*this), (T)z());
}

constexpr global_coords global_coords::operator+(Vector2i vec) const noexcept
{
    return { uint32_t((int64_t)x+vec[0]), uint32_t((int64_t)y+vec[1]), nullptr };
}

constexpr global_coords& global_coords::operator+=(Vector2i vec) noexcept
{
    x = uint32_t((int64_t)x+vec[0]);
    y = uint32_t((int64_t)y+vec[1]);
    return *this;
}

constexpr global_coords global_coords::operator-(Vector2i vec) const noexcept
{
    return { uint32_t((int64_t)x-vec[0]), uint32_t((int64_t)y-vec[1]), nullptr };
}

constexpr global_coords& global_coords::operator-=(Vector2i vec) noexcept
{
    x = uint32_t((int64_t)x-vec[0]);
    y = uint32_t((int64_t)y-vec[1]);
    return *this;
}

constexpr Vector2i global_coords::operator-(global_coords other) const noexcept
{
    return Vector2i(*this) - Vector2i(other);
}

} // namespace floormat
