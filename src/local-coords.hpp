#pragma once
#include "compat/assert.hpp"
#include "tile-defs.hpp"
#include <concepts>

namespace floormat {

struct local_coords final {
    std::uint8_t x : 4 = 0, y : 4 = 0;
    explicit constexpr local_coords(std::size_t idx) noexcept;
    constexpr local_coords() noexcept = default;
    template<std::integral T> requires (sizeof(T) <= sizeof(std::size_t))
    constexpr local_coords(T x, T y) noexcept;
    constexpr local_coords(std::uint8_t x, std::uint8_t y) noexcept : x{x}, y{y} {}
    constexpr std::uint8_t to_index() const noexcept { return y*TILE_MAX_DIM + x; }
};

constexpr local_coords::local_coords(std::size_t index) noexcept :
      x{(std::uint8_t)(index % TILE_MAX_DIM)},
      y{(std::uint8_t)(index / TILE_MAX_DIM)}
{
    fm_assert(index < TILE_COUNT);
}

template<std::integral T>
requires (sizeof(T) <= sizeof(std::size_t))
constexpr local_coords::local_coords(T x, T y) noexcept
    : x{(std::uint8_t)x}, y{(std::uint8_t)y}
{
    fm_assert((std::size_t)x < TILE_MAX_DIM && (std::size_t)y < TILE_MAX_DIM);
}

} // namespace floormat
