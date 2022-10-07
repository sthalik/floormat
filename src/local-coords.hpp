#pragma once
#include "compat/assert.hpp"
#include "tile-defs.hpp"
#include <cstdint>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector3.h>

namespace Magnum::Examples {

struct local_coords final {
    std::uint8_t x = 0, y = 0;
    explicit constexpr local_coords(std::size_t idx);
    constexpr local_coords() = default;
    constexpr local_coords(std::size_t x, std::size_t y);
    constexpr local_coords(std::uint8_t x, std::uint8_t y) : x{x}, y{y} {}
    constexpr std::size_t to_index() const { return y*TILE_MAX_DIM + x; }
};

constexpr local_coords::local_coords(std::size_t index) :
      x{(std::uint8_t)(index % TILE_MAX_DIM)},
      y{(std::uint8_t)(index / TILE_MAX_DIM)}
{
    ASSERT(index < TILE_COUNT);
}

constexpr local_coords::local_coords(std::size_t x, std::size_t y)
    : x{(std::uint8_t)x}, y{(std::uint8_t)y}
{
    ASSERT(x <= 0xff && y <= 0xff);
}

} // namespace Magnum::Examples
