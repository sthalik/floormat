#pragma once
#include "src/chunk.hpp"
#include "compat/assert.hpp"

namespace floormat {

struct chunk_coords final {
    std::int16_t x = 0, y = 0;

    constexpr bool operator==(const chunk_coords& other) const noexcept = default;
};

struct global_coords final {
    std::uint32_t x = 0, y = 0;

    constexpr global_coords(chunk_coords c, local_coords xy) :
        x{ std::uint32_t(c.x + (1 << 15)) << 4 | (xy.x & 0x0f) },
        y{ std::uint32_t(c.y + (1 << 15)) << 4 | (xy.y & 0x0f) }
    {}
    constexpr global_coords(std::uint32_t x, std::uint32_t y) noexcept : x{x}, y{y} {}
    constexpr global_coords() noexcept = default;

    constexpr local_coords local() const noexcept;
    constexpr chunk_coords chunk() const noexcept;

    constexpr bool operator==(const global_coords& other) const noexcept = default;
};

constexpr local_coords global_coords::local() const noexcept
{
    return { (std::uint8_t)(x % TILE_MAX_DIM), (std::uint8_t)(y % TILE_MAX_DIM) };
}


constexpr chunk_coords global_coords::chunk() const noexcept
{
    return {
        (std::int16_t)(std::int32_t(x >> 4) - (1 << 15)),
        (std::int16_t)(std::int32_t(y >> 4) - (1 << 15)),
    };
}

} // namespace floormat
