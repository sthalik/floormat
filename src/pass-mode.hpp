#pragma once
#include <cstdint>

namespace floormat {

enum class pass_mode : std::uint8_t { blocked, see_through, shoot_through, pass, };
constexpr inline std::uint8_t pass_mode_COUNT = 4;
constexpr inline std::uint8_t pass_mode_BITS = 2;

} // namespace floormat
