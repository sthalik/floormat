#pragma once
#include "assert.hpp"
#include <bit>
#include <cfloat>
#include <cmath>

namespace floormat {
constexpr int fpclassify(float x)
{
    if consteval
    {
        const uint32_t bits = std::bit_cast<uint32_t>(x);
        const uint32_t exp = (bits >> 23) & 0xFF;
        const uint32_t mantissa = bits & 0x7FFFFF;
        if (exp == 0xFF)
            return mantissa == 0 ? FP_INFINITE : FP_NAN;
        else if (exp == 0x00)
            return mantissa == 0 ? FP_ZERO : FP_SUBNORMAL;
        else
            return FP_NORMAL;
    }
    else
    {
        return std::fpclassify(x);
    }
}

constexpr float nth_float(float x, uint32_t n)
{
    const uint32_t u = std::bit_cast<uint32_t>(x);
    // map to lexicographic uint32_t order
    const uint32_t mapped = (u & 0x80000000u) != 0u ? ~u : (u ^ 0x80000000u);
    constexpr uint32_t HOLE_LO = 0x7F800000u; // mapped(-max_subnormal)
    constexpr uint32_t HOLE_HI = 0x807FFFFFu; // mapped(+max_subnormal)
    constexpr uint32_t HOLE_SIZE = 0x01000000u; // neg subnorms + -0 + +0 + pos subnorms

    uint32_t base = mapped;
    if (base >= HOLE_LO && base <= HOLE_HI)
        base = HOLE_LO - 1u; // x was zero/subnormal

    uint32_t newMapped = base + n;
    if (base < HOLE_LO && newMapped >= HOLE_LO)
    {
        fm_debug_assert(newMapped <= UINT32_MAX - HOLE_SIZE);
        newMapped += HOLE_SIZE; // skip denorm block
    }

    const uint32_t newU = (newMapped & 0x80000000u) != 0 ? newMapped ^ 0x80000000u : ~newMapped;
    const float ret = std::bit_cast<float>(newU);
    fm_debug_assert(fpclassify(ret) == FP_NORMAL && newMapped >= mapped);
    return ret;
}

} // namespace floormat
