#pragma once
#include "compat/assert.hpp"
#include "tile-defs.hpp"
#include <concepts>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

namespace floormat {

struct local_coords final {
    uint8_t x : 4 = 0, y : 4 = 0;
    explicit constexpr local_coords(size_t idx) noexcept;
    constexpr local_coords() noexcept = default;
    template<typename T> requires (std::is_integral_v<T> && sizeof(T) <= sizeof(size_t))
    constexpr local_coords(T x, T y) noexcept;
    constexpr local_coords(uint8_t x, uint8_t y) noexcept : x{x}, y{y} {}
    constexpr uint8_t to_index() const noexcept { return y*TILE_MAX_DIM + x; }
    constexpr bool operator==(const local_coords&) const noexcept = default;

    template<typename T> explicit constexpr operator Math::Vector2<T>() const noexcept { return Math::Vector2<T>(T(x), T(y)); }
    template<typename T> explicit constexpr operator Math::Vector3<T>() const noexcept { return Math::Vector3<T>(T(x), T(y), T(0)); }
};

constexpr local_coords::local_coords(size_t index) noexcept :
      x{(uint8_t)(index % TILE_MAX_DIM)},
      y{(uint8_t)(index / TILE_MAX_DIM)}
{
    fm_assert(index < TILE_COUNT);
}

template<typename T>
requires (std::is_integral_v<T> && sizeof(T) <= sizeof(size_t))
constexpr local_coords::local_coords(T x, T y) noexcept
    : x{(uint8_t)x}, y{(uint8_t)y}
{
    fm_assert((size_t)x < TILE_MAX_DIM && (size_t)y < TILE_MAX_DIM);
}

} // namespace floormat
