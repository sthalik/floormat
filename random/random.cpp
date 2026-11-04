#include "random.hpp"
#include <bit>

namespace floormat::Random {

namespace {

constexpr u64 ranlux48_mask = (u64{1} << 48) - 1;
constexpr u32 ranlux48_short_lag = 5, ranlux48_long_lag = 12;
constexpr u32 ranlux48_block = 389, ranlux48_used = 11;

u64 next_ranlux48_base(ranlux48& g)
{
    // x[i] holds X[k-12]. X[k-5] was written 5 steps ago.
    const u32 j = g.i < ranlux48_short_lag ? g.i + ranlux48_long_lag - ranlux48_short_lag
                                                : g.i - ranlux48_short_lag;
    const u64 a = g.x[j], b = g.x[g.i] + g.c;
    const u64 y = (a - b) & ranlux48_mask;
    g.c = a < b;
    g.x[g.i] = y;
    g.i = g.i + 1 == ranlux48_long_lag ? 0 : g.i + 1;
    return y;
}

u64 mix13(u64 z)
{
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

} // namespace

u64 mix(u64 x)
{
    constexpr u64 C = 0xbea225f9eb34556d;
    x ^= x >> 32;
    x *= C;
    x ^= x >> 29;
    x *= C;
    x ^= x >> 32;
    x *= C;
    x ^= x >> 29;
    return x;
}

u64 next(splitmix64& state)
{
    return mix13(state.x += 0x9e3779b97f4a7c15);
}

u64 next(xoshiro256starstar& state)
{
    auto& s = state.s;
    const u64 result = std::rotl(s[1] * 5, 7) * 9;
    const u64 t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = std::rotl(s[3], 45);
    return result;
}

u64 next(ranlux48& state)
{
    if (state.n >= ranlux48_used)
    {
        for (u32 k = 0; k < ranlux48_block - ranlux48_used; k++)
            (void)next_ranlux48_base(state);
        state.n = 0;
    }
    state.n++;
    return next_ranlux48_base(state);
}

void seed(splitmix64& state, u64 value)
{
    state.x = value;
}

// The authors' suggestion for a 64-bit seed. The all-zero state never
// leaves zero, and states with few set bits give runs of zero outputs.
void seed(xoshiro256starstar& state, u64 value)
{
    splitmix64 g{value};
    for (auto& x : state.s)
        x = next(g);
}

void seed(ranlux48& state, u64 value)
{
    constexpr u64 m = 2147483563, default_seed = 19780503;
    u64 lcg = value == 0 ? default_seed : value % m;
    if (lcg == 0)
        lcg = 1;
    for (auto& x : state.x)
    {
        const u64 lo = lcg = lcg * 40014 % m;
        const u64 hi = lcg = lcg * 40014 % m;
        x = (lo | hi << 32) & ranlux48_mask;
    }
    state.i = 0;
    state.c = state.x[ranlux48_long_lag - 1] == 0;
    state.n = 0;
}

} // namespace floormat::Random
