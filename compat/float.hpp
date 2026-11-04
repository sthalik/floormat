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
        const u32 bits = std::bit_cast<u32>(x);
        const u32 exp = (bits >> 23) & 0xFF;
        const u32 mantissa = bits & 0x7FFFFF;
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

constexpr float nth_float(float x, u32 n)
{
    const u32 u = std::bit_cast<u32>(x);
    // map to lexicographic u32 order
    const u32 mapped = (u & 0x80000000u) != 0u ? ~u : (u ^ 0x80000000u);
    constexpr u32 HOLE_LO = 0x7F800000u; // mapped(-max_subnormal)
    constexpr u32 HOLE_HI = 0x807FFFFFu; // mapped(+max_subnormal)
    constexpr u32 HOLE_SIZE = 0x01000000u; // neg subnorms + -0 + +0 + pos subnorms

    u32 base = mapped;
    if (base >= HOLE_LO && base <= HOLE_HI)
        base = HOLE_LO - 1u; // x was zero/subnormal

    u32 newMapped = base + n;
    if (base < HOLE_LO && newMapped >= HOLE_LO)
    {
        fm_debug3_assert(newMapped <= UINT32_MAX - HOLE_SIZE);
        newMapped += HOLE_SIZE; // skip denorm block
    }

    const u32 newU = (newMapped & 0x80000000u) != 0 ? newMapped ^ 0x80000000u : ~newMapped;
    const float ret = std::bit_cast<float>(newU);
    fm_debug3_assert(fpclassify(ret) == FP_NORMAL && newMapped >= mapped);
    return ret;
}

} // namespace floormat
