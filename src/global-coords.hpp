#pragma once
#include "local-coords.hpp"
#include "compat/assert.hpp"
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

struct chunk_coords final {
    std::int16_t x = 0, y = 0;

    constexpr bool operator==(const chunk_coords& other) const noexcept = default;
};

struct global_coords final {
    std::uint32_t x = 1 << 15, y = 1 << 15;

    constexpr global_coords(chunk_coords c, local_coords xy) :
        x{ std::uint32_t(c.x + (1 << 15)) << 4 | (xy.x & 0x0f) },
        y{ std::uint32_t(c.y + (1 << 15)) << 4 | (xy.y & 0x0f) }
    {}
    constexpr global_coords(std::uint32_t x, std::uint32_t y) noexcept : x{x}, y{y} {}
    constexpr global_coords(std::int32_t x, std::int32_t y) noexcept :
          x{std::uint32_t(x + (1 << 15))}, y{std::uint32_t(y + (1 << 15))}
    {}
    constexpr global_coords() noexcept = default;

    constexpr local_coords local() const noexcept;
    constexpr chunk_coords chunk() const noexcept;

    constexpr Vector2i to_signed() const noexcept;

    constexpr bool operator==(const global_coords& other) const noexcept = default;
};

constexpr local_coords global_coords::local() const noexcept
{
    return {
        std::uint8_t(x & 0x0f),
        std::uint8_t(y & 0x0f),
    };
}

constexpr chunk_coords global_coords::chunk() const noexcept
{
    return {
        std::int16_t((x - (1 << 15)) >> 4),
        std::int16_t((y - (1 << 15)) >> 4),
    };
}

constexpr Vector2i global_coords::to_signed() const noexcept
{
    return {
        std::int32_t(x - (1 << 15)),
        std::int32_t(y - (1 << 15)),
    };
}

} // namespace floormat
