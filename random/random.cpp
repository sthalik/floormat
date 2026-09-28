#include "random.hpp"
#include <bit>

namespace floormat::Random {

namespace {

constexpr uint64_t ranlux48_mask = (uint64_t{1} << 48) - 1;
constexpr uint32_t ranlux48_short_lag = 5, ranlux48_long_lag = 12;
constexpr uint32_t ranlux48_block = 389, ranlux48_used = 11;

uint64_t next_ranlux48_base(ranlux48& g)
{
    // x[i] holds X[k-12]. X[k-5] was written 5 steps ago.
    const uint32_t j = g.i < ranlux48_short_lag ? g.i + ranlux48_long_lag - ranlux48_short_lag
                                                : g.i - ranlux48_short_lag;
    const uint64_t a = g.x[j], b = g.x[g.i] + g.c;
    const uint64_t y = (a - b) & ranlux48_mask;
    g.c = a < b;
    g.x[g.i] = y;
    g.i = g.i + 1 == ranlux48_long_lag ? 0 : g.i + 1;
    return y;
}

uint64_t mix13(uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

} // namespace

uint64_t mix(uint64_t x)
{
    constexpr uint64_t C = 0xbea225f9eb34556d;
    x ^= x >> 32;
    x *= C;
    x ^= x >> 29;
    x *= C;
    x ^= x >> 32;
    x *= C;
    x ^= x >> 29;
    return x;
}

uint64_t next(splitmix64& state)
{
    return mix13(state.x += 0x9e3779b97f4a7c15);
}

uint64_t next(xoshiro256starstar& state)
{
    auto& s = state.s;
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

uint64_t next(ranlux48& state)
{
    if (state.n >= ranlux48_used)
    {
        for (uint32_t k = 0; k < ranlux48_block - ranlux48_used; k++)
            (void)next_ranlux48_base(state);
        state.n = 0;
    }
    state.n++;
    return next_ranlux48_base(state);
}

void seed(splitmix64& state, uint64_t value)
{
    state.x = value;
}

// The authors' suggestion for a 64-bit seed. The all-zero state never
// leaves zero, and states with few set bits give runs of zero outputs.
void seed(xoshiro256starstar& state, uint64_t value)
{
    splitmix64 g{value};
    for (auto& x : state.s)
        x = next(g);
}

void seed(ranlux48& state, uint64_t value)
{
    constexpr uint64_t m = 2147483563, default_seed = 19780503;
    uint64_t lcg = value == 0 ? default_seed : value % m;
    if (lcg == 0)
        lcg = 1;
    for (auto& x : state.x)
    {
        const uint64_t lo = lcg = lcg * 40014 % m;
        const uint64_t hi = lcg = lcg * 40014 % m;
        x = (lo | hi << 32) & ranlux48_mask;
    }
    state.i = 0;
    state.c = state.x[ranlux48_long_lag - 1] == 0;
    state.n = 0;
}

} // namespace floormat::Random
