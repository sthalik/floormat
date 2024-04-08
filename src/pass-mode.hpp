#pragma once

namespace floormat {

enum class pass_mode : unsigned char { blocked, see_through, shoot_through, pass, COUNT };
constexpr inline unsigned char pass_mode_BITS = 2;

} // namespace floormat
