#pragma once
#include "splitmix64.hpp"
#include <bit>

namespace floormat {

// Use the raw outputs. <random> distributions and std::shuffle give different
// sequences on each standard library.
struct xoshiro256starstar
{
    using result_type = uint64_t;

    uint64_t s[4];

    // The authors' suggestion for a 64-bit seed. The all-zero state never
    // leaves zero, and states with few set bits give runs of zero outputs.
    constexpr explicit xoshiro256starstar(uint64_t seed) noexcept
    {
        splitmix64 g{seed};
        for (auto& x : s)
            x = g();
    }

    static constexpr uint64_t min() noexcept { return 0; }
    static constexpr uint64_t max() noexcept { return (uint64_t)-1; }

    constexpr uint64_t operator()() noexcept
    {
        const uint64_t result = std::rotl(s[1] * 5, 7) * 9;
        const uint64_t t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = std::rotl(s[3], 45);
        return result;
    }
};

static_assert(std::uniform_random_bit_generator<xoshiro256starstar>);

} // namespace floormat
