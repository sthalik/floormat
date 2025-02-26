#pragma once
#include "compat/assert.hpp"
#include "tile-defs.hpp"
#include <type_traits>

namespace Magnum::Math {
template<class T> class Vector2;
template<class T> class Vector3;
} // namespace Magnum::Math

namespace floormat {

struct local_coords final {
    uint8_t x : 4 = 0, y : 4 = 0;

    explicit constexpr local_coords(size_t idx) noexcept;
    constexpr local_coords() noexcept = default;
    constexpr local_coords(uint8_t x, uint8_t y) noexcept : x{x}, y{y} {}

    template<typename T> requires (std::is_integral_v<T> && sizeof(T) <= sizeof(size_t))
    constexpr local_coords(T x, T y) noexcept;

    template<typename T> requires requires (const Math::Vector2<T>& vec) { local_coords{vec.x(), vec.y()}; }
    constexpr local_coords(Math::Vector2<T> vec) noexcept : local_coords{vec.x(), vec.y()} {}

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
