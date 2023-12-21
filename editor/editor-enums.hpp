#pragma once

namespace floormat {

enum class editor_mode : unsigned char {
    none, floor, walls, scenery, vobj, tests,
};

enum class editor_snap_mode : unsigned char {
    none       = 0,
    horizontal = 1 << 0,
    vertical   = 1 << 1,
};

} // namespace floormat
