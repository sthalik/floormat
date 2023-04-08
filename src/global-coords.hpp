#pragma once
#include "local-coords.hpp"
#include "compat/assert.hpp"
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

namespace floormat {

struct chunk_coords final {
    int16_t x = 0, y = 0;

    constexpr bool operator==(const chunk_coords& other) const noexcept = default;
    constexpr Vector2i operator-(chunk_coords other) const noexcept;

    template<typename T>
    requires (std::is_floating_point_v<T> || std::is_integral_v<T> && (sizeof(T) > sizeof(x) || std::is_same_v<T, std::decay_t<decltype(x)>>))
    explicit constexpr operator Math::Vector2<T>() const noexcept { return Math::Vector2<T>(T(x), T(y)); }

    template<typename T>
    requires (std::is_floating_point_v<T> || std::is_integral_v<T> && (sizeof(T) > sizeof(x) || std::is_same_v<T, std::decay_t<decltype(x)>>))
    explicit constexpr operator Math::Vector3<T>() const noexcept { return Math::Vector3<T>(T(x), T(y), T(0)); }
};

constexpr Vector2i chunk_coords::operator-(chunk_coords other) const noexcept
{
    return { Int{x} - other.x, Int{y} - other.y };
}

struct chunk_coords_ final {
    int16_t x = 0, y = 0;
    int8_t z = 0;

    constexpr chunk_coords_() noexcept = default;
    constexpr chunk_coords_(int16_t x, int16_t y, int8_t z) noexcept : x{x}, y{y}, z{z} {}
    constexpr chunk_coords_(chunk_coords c, int8_t z) noexcept : x{c.x}, y{c.y}, z{z} {}
    constexpr bool operator==(const chunk_coords_&) const noexcept = default;
};

constexpr inline int8_t chunk_min_z = -1, chunk_max_z = 14;

struct global_coords final {
    using u0 = std::integral_constant<uint32_t, (1<<15)>;
    using s0 = std::integral_constant<int32_t, int32_t(u0::value)>;
    using z0 = std::integral_constant<int32_t, (1 << 0)>;
    using z_mask = std::integral_constant<uint32_t, (1u << 4) - 1u << 20>;
    uint32_t x = u0::value<<4|z0::value<<20, y = u0::value<<4;

    constexpr global_coords() noexcept = default;
    constexpr global_coords(chunk_coords c, local_coords xy, int8_t z) noexcept :
        x{
            uint32_t((c.x + s0::value) << 4) | (xy.x & 0x0f) |
            uint32_t(((int)z + z0::value) & 0x0f) << 20
        },
        y{ uint32_t((c.y + s0::value) << 4) | (xy.y & 0x0f) }
    {}
    constexpr global_coords(uint32_t x, uint32_t y, std::nullptr_t) noexcept : x{x}, y{y} {}
    constexpr global_coords(int32_t x, int32_t y, int8_t z) noexcept :
        x{uint32_t(x + (s0::value<<4)) | uint32_t(((z + z0::value) & 0x0f) << 20)},
        y{uint32_t(y + (s0::value<<4))}
    {}
    constexpr global_coords(chunk_coords_ c, local_coords xy) noexcept :
        global_coords{chunk_coords{c.x, c.y}, xy, c.z}
    {}

    constexpr local_coords local() const noexcept;
    constexpr chunk_coords chunk() const noexcept;
    constexpr int8_t z() const noexcept;

    constexpr Vector2i to_signed() const noexcept;
    constexpr Vector3i to_signed3() const noexcept;
    constexpr bool operator==(const global_coords& other) const noexcept = default;

    constexpr global_coords operator+(Vector2i vec) const noexcept;
    constexpr global_coords operator-(Vector2i vec) const noexcept;
    constexpr global_coords& operator+=(Vector2i vec) noexcept;
    constexpr global_coords& operator-=(Vector2i vec) noexcept;
    constexpr Vector2i operator-(global_coords other) const noexcept;
};

constexpr local_coords global_coords::local() const noexcept
{
    return { uint8_t(x & 0x0f), uint8_t(y & 0x0f), };
}

constexpr chunk_coords global_coords::chunk() const noexcept
{
    return { int16_t(int32_t(((x & ~z_mask::value)>>4) - u0::value)), int16_t(int32_t((y>>4) - u0::value)), };
}

constexpr int8_t global_coords::z() const noexcept
{
    return ((x >> 20) & 0x0f) - z0::value;
}

constexpr Vector2i global_coords::to_signed() const noexcept
{
    return { int32_t((x & ~z_mask::value) - (s0::value<<4)), int32_t(y - (s0::value<<4)), };
}

constexpr Vector3i global_coords::to_signed3() const noexcept
{
    return Vector3i(to_signed(), 0);
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
    return to_signed() - other.to_signed();
}

} // namespace floormat
