#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector3.h>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace Magnum::Examples {

struct tile_atlas;
constexpr inline Vector3 TILE_SIZE = { 64, 64, 64 };
constexpr inline std::size_t TILE_MAX_DIM = 16;
constexpr inline std::size_t TILE_COUNT = TILE_MAX_DIM*TILE_MAX_DIM;

struct tile_image final
{
    std::shared_ptr<tile_atlas> atlas;
    std::uint8_t variant = 0xff;

    explicit operator bool() const noexcept { return !!atlas; }
};

struct tile final
{
    enum class pass_mode : std::uint8_t { pass_blocked, pass_ok, pass_shoot_through, };
    using enum pass_mode;

    tile_image ground_image, wall_west, wall_north;
    pass_mode passability = pass_shoot_through;
};

struct local_coords final {
    std::uint8_t x = 0, y = 0;
    constexpr local_coords() = default;
    local_coords(std::size_t x, std::size_t y);
    constexpr local_coords(std::uint8_t x, std::uint8_t y) : x{x}, y{y} {}
    constexpr std::size_t to_index() const { return y*TILE_MAX_DIM + x; }
};

} //namespace Magnum::Examples
