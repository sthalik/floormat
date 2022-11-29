#pragma once
#include "local-coords.hpp"
#include "compat/assert.hpp"
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

struct chunk_coords final {
    std::int16_t x = 0, y = 0;

    constexpr bool operator==(const chunk_coords& other) const noexcept = default;
    constexpr Vector2i operator-(chunk_coords other) const noexcept;
};

constexpr Vector2i chunk_coords::operator-(chunk_coords other) const noexcept
{
    return { Int{x} - other.x, Int{y} - other.y };
}

struct global_coords final {
    static constexpr std::uint32_t _0u = 1 << 15;
    static constexpr auto _0s = std::int32_t(_0u);

    std::uint32_t x = _0u, y = _0u;

    constexpr global_coords() noexcept = default;
    constexpr global_coords(chunk_coords c, local_coords xy) :
        x{ std::uint32_t(c.x + _0s) << 4 | (xy.x & 0x0f) },
        y{ std::uint32_t(c.y + _0s) << 4 | (xy.y & 0x0f) }
    {}
    constexpr global_coords(std::uint32_t x, std::uint32_t y) noexcept : x{x}, y{y} {}
    constexpr global_coords(std::int32_t x, std::int32_t y) noexcept :
          x{std::uint32_t(x + _0s)}, y{std::uint32_t(y + _0s)}
    {}

    constexpr local_coords local() const noexcept;
    constexpr chunk_coords chunk() const noexcept;

    constexpr Vector2i to_signed() const noexcept;
    constexpr bool operator==(const global_coords& other) const noexcept = default;

    constexpr global_coords operator+(Vector2i vec) const noexcept;
    constexpr global_coords operator-(Vector2i vec) const noexcept;
    constexpr global_coords& operator+=(Vector2i vec) noexcept;
    constexpr global_coords& operator-=(Vector2i vec) noexcept;
    constexpr Vector2i operator-(global_coords other) const noexcept;
};

constexpr local_coords global_coords::local() const noexcept
{
    return { std::uint8_t(x & 0x0f), std::uint8_t(y & 0x0f), };
}

constexpr chunk_coords global_coords::chunk() const noexcept
{
    return { std::int16_t((x - _0u) >> 4), std::int16_t((y - _0u) >> 4), };
}

constexpr Vector2i global_coords::to_signed() const noexcept
{
    return { std::int32_t(x - _0s), std::int32_t(y - _0s), };
}

constexpr global_coords global_coords::operator+(Vector2i vec) const noexcept
{
    return { std::uint32_t((std::int64_t)x+vec[0]), std::uint32_t((std::int64_t)y+vec[1]) };
}

constexpr global_coords& global_coords::operator+=(Vector2i vec) noexcept
{
    x = std::uint32_t((std::int64_t)x+vec[0]);
    y = std::uint32_t((std::int64_t)y+vec[1]);
    return *this;
}

constexpr global_coords global_coords::operator-(Vector2i vec) const noexcept
{
    return { std::uint32_t((std::int64_t)x-vec[0]), std::uint32_t((std::int64_t)y-vec[1]) };
}

constexpr global_coords& global_coords::operator-=(Vector2i vec) noexcept
{
    x = std::uint32_t((std::int64_t)x-vec[0]);
    y = std::uint32_t((std::int64_t)y-vec[1]);
    return *this;
}

constexpr Vector2i global_coords::operator-(global_coords other) const noexcept
{
    return to_signed() - other.to_signed();
}

} // namespace floormat
