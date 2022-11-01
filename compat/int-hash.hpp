#pragma once
#include "integer-types.hpp"

namespace floormat {

constexpr inline std::size_t int_hash(std::size_t x) noexcept
{
    if constexpr(sizeof(std::size_t) == 4)
    {
        // by Chris Wellons <https://nullprogram.com/blog/2018/07/31/>
        x ^= x >> 15;
        x *= 0x2c1b3c6dU;
        x ^= x >> 12;
        x *= 0x297a2d39U;
        x ^= x >> 15;
    }
    else if constexpr(sizeof(std::size_t) == 8)
    {
        // splitmix64 by George Marsaglia
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9U;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebU;
        x ^= x >> 31;
    }

    return x;
}

} // namespace floormat
