#pragma once

namespace floormat {

enum class editor_mode : unsigned char {
    none, floor, walls, scenery,
};

enum class editor_wall_rotation : std::uint8_t {
    N, W,
};

enum class editor_snap_mode : std::uint8_t {
    none       = 0,
    horizontal = 1 << 0,
    vertical   = 1 << 1,
};

} // namespace floormat
