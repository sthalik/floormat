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

// Grid::Pass::Params::min_pass compares with >=
static_assert(pass_mode::blocked < pass_mode::see_through &&
              pass_mode::see_through < pass_mode::shoot_through &&
              pass_mode::shoot_through < pass_mode::pass);

} // namespace floormat
