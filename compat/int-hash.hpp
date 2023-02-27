#pragma once
#include <cstddef>
#include <bit>

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
        // NASAM by Pelle Evensen <https://mostlymangling.blogspot.com/2020/01/nasam-not-another-strange-acronym-mixer.html>
        x ^= std::rotr(x, 25) ^ std::rotr(x, 47);
        x *= 0x9E6C63D0676A9A99UL;
        x ^= x >> 23 ^ x >> 51;
        x *= 0x9E6D62D06F6A9A9BUL;
        x ^= x >> 23 ^ x >> 51;
    }

    return x;
}

} // namespace floormat
