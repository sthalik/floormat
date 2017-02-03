#include "xorshift.hpp"
#include "murmur.hpp"
#include <ctime>
#include <climits>
#include <cinttypes>

using seed = xorshift::seed;
using u64 = xorshift::u64;

u64 xorshift::operator()()
{
    u64 x, y;
    std::tie(x, y) = ss;
    //ss.s[0] = y;
    x ^= x << 23; // a
    const u64 y_ = x ^ y ^ (x >> 17) ^ (y >> 26); // b, c
    ss = std::tie(y, y_);

    return y_ + y;
}

void xorshift::reseed(const seed& value)
{
    ss = value;
}

xorshift::seed xorshift::sanitize_seed(const xorshift::seed& value)
{
    if (value == default_seed)
    {
        std::time_t t = std::time(nullptr);
        return make_seed_from_bytes(&t, sizeof(t));
    }
    return value;
}

constexpr seed xorshift::default_seed;

seed xorshift::make_seed_from_bytes(const void* in, unsigned len)
{
    return sanitize_seed(murmur128(in, len, 0));
}
