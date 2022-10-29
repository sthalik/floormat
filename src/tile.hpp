#pragma once
#include "compat/defs.hpp"
#include "tile-image.hpp"

namespace floormat {

struct tile final
{
    enum pass_mode : std::uint8_t { pass_ok, pass_blocked, pass_shoot_through, };

    tile_image ground, wall_north, wall_west;
    pass_mode passability = pass_ok;

    constexpr tile() = default;

    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(tile);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(tile);
};

bool operator==(const tile& a, const tile& b) noexcept;

} //namespace floormat
