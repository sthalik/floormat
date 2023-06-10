#pragma once

namespace floormat {

enum class light_falloff : uint8_t {
    constant = 1, linear = 0, quadratic = 2,
};

constexpr inline light_falloff light_falloff_COUNT{3};
constexpr inline uint8_t light_falloff_BITS = 3;
constexpr inline uint8_t light_falloff_MASK = (1 << light_falloff_BITS)-1;

} // namespace floormat
