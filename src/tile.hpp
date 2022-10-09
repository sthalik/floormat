#pragma once
#include "compat/defs.hpp"
#include "compat/assert.hpp"
#include "tile-defs.hpp"
#include <cstdint>
#include <memory>

namespace floormat {

struct tile_atlas;

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

    tile_image ground_image, wall_north, wall_west;
    pass_mode passability = pass_shoot_through;

    constexpr tile() = default;
    tile(tile&&) = default;

    DECLARE_DEPRECATED_COPY_ASSIGNMENT(tile);
};

} //namespace floormat
