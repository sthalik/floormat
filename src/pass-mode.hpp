#pragma once
#include <cstdint>

namespace floormat {

enum class pass_mode : std::uint8_t { shoot_through, pass, blocked, see_through };
constexpr inline std::uint8_t pass_mode_COUNT = std::uint8_t(pass_mode::see_through) + 1;
static_assert(pass_mode_COUNT == 4);

} // namespace floormat
