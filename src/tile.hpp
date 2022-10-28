#pragma once
#include "compat/defs.hpp"
#include "tile-image.hpp"

namespace floormat {

struct tile final
{
    enum pass_mode : std::uint8_t { pass_blocked, pass_ok, pass_shoot_through, };

    tile_image ground_image, wall_north, wall_west;
    pass_mode passability = pass_shoot_through;

    constexpr tile() = default;

    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(tile);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(tile);
};

} //namespace floormat
