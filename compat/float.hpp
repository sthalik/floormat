#pragma once
#include "assert.hpp"
#include <bit>
#include <cstdint>
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
    const uint32_t mapped = (u & UINT32_C(0x80000000)) != 0u ? ~u : (u ^ UINT32_C(0x80000000));
    const uint32_t newMapped = mapped + n;
    const uint32_t newU = (newMapped & UINT32_C(0x80000000)) != 0 ? newMapped ^ UINT32_C(0x80000000) : ~newMapped;
    const float ret = std::bit_cast<float>(newU);
#ifndef FM_NO_DEBUG
    const int type = fpclassify(x);
#endif
    fm_debug_assert((type == FP_NORMAL || type == FP_ZERO) && newMapped >= mapped);
    return ret;
}

constexpr float nth_float(uint32_t n) { return nth_float(FLT_MIN, n); }

} // namespace floormat
