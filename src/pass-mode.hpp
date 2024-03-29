#pragma once

namespace floormat {

enum class pass_mode : unsigned char { blocked, see_through, shoot_through, pass, };
constexpr inline pass_mode pass_mode_COUNT{4};
constexpr inline unsigned char pass_mode_BITS = 2;

} // namespace floormat
