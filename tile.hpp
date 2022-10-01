#pragma once
#include "tile-atlas.hpp"
#include "hash.hpp"
#include "defs.hpp"

#include <concepts>
#include <cstddef>
#include <tuple>
#include <array>
#include <memory>
#include <unordered_map>
#include <utility>

namespace Magnum::Examples {

constexpr inline Vector3 TILE_SIZE = { 50, 50, 50 };

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

} //namespace Magnum::Examples
