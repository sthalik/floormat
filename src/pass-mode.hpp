#pragma once

namespace floormat {

enum class pass_mode : unsigned char {
    blocked,
    see_through,   // can see and can't shoot through (e.g. bulletproof glass)
    shoot_through, // can see and shoot through
    pass,
    COUNT
};
constexpr inline unsigned char pass_mode_BITS = 2;

} // namespace floormat
