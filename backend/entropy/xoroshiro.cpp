/*  Written in 2016 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

#include "xoroshiro.hpp"
#include "splitmix64.hpp"
#include "murmur.hpp"
#include "util.hpp"

#include <cstddef>
#include <cinttypes>
#include <tuple>

using seed = xoroshiro::seed;
using u64 = std::uint64_t;

#if defined _WIN32
#   define rotl _rotl64
#else
static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}
#endif

u64 xoroshiro::operator()()
{
    u64 s0, s1;
    std::tie(s0, s1) = ss;
    const u64 result = s0 + s1;

    s1 ^= s0;

    ss = seed(rotl(s0, 55) ^ s1 ^ (s1 << 14), rotl(s1, 36));
    return result;
}

xoroshiro::xoroshiro(const void* buf, unsigned len) : ss(make_seed_from_bytes(buf, len))
{
}

seed xoroshiro::make_seed_from_bytes(const void* in, unsigned len)
{
    return murmur128(in, len, 0);
}

xoroshiro::xoroshiro(const u64& s) : ss(progn(splitmix64 gen(s); u64 s0 = gen(); return seed(s0, gen());))
{
}

