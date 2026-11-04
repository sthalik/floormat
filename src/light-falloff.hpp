#pragma once

namespace floormat {

enum class light_falloff : u8 {
    constant = 1, linear = 0, quadratic = 2, COUNT,
};

constexpr inline u8 light_falloff_BITS = 3;
constexpr inline u8 light_falloff_MASK = (1 << light_falloff_BITS)-1;

} // namespace floormat
