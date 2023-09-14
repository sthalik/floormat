#include "app.hpp"
#include "compat/int-hash.hpp"
#include "src/global-coords.hpp"
#include <cmath>
#include <cstdio>
#include <memory>
#include <algorithm>
#include <utility>
#include <tsl/robin_hash.h>

namespace floormat
{

namespace
{

template <uint8_t a, uint32_t b, uint8_t c, uint32_t d, uint8_t e> [[maybe_unused]] CORRADE_ALWAYS_INLINE uint32_t hash32(uint32_t x)
{
    x++;
    x ^= x >> a;
    x *= b;
    x ^= x >> c;
    x *= d;
    x ^= x >> e;

    return x;
}

template <uint8_t a, uint32_t b, uint8_t c, uint32_t d, uint8_t e> [[maybe_unused]] uint32_t hash32_(uint64_t x)
{
    return hash32<a, b, c, d, e>((uint32_t)(x * 65521 >> 32)) ^ hash32<a, b, c, d, e>((uint32_t)x);
}

template <uint8_t a, uint64_t b, uint8_t c, uint64_t d, uint8_t e> [[maybe_unused]] uint64_t hash64(uint64_t x)
{
    x++;
    x ^= x >> a;
    x *= b;
    x ^= x >> c;
    x *= d;
    x ^= x >> e;
    return x;
}

template <uint8_t a, uint32_t b, uint8_t c, uint32_t d, uint8_t e, uint32_t f, uint8_t g> [[maybe_unused]] uint32_t triple32(uint32_t x)
{
    x++;
    x ^= x >> a;
    x *= b;
    x ^= x >> c;
    x *= d;
    x ^= x >> e;
    x *= f;
    x ^= x >> g;
    return x;
}

template <uint8_t a, uint32_t b, uint8_t c, uint32_t d, uint8_t e, uint32_t f, uint8_t g> [[maybe_unused]] uint32_t triple32_(uint64_t x)
{
    return triple32<a, b, c, d, e, f, g>((uint32_t)(x * 65521 >> 32)) ^ triple32<a, b, c, d, e, f, g>((uint32_t)x);
}

[[maybe_unused]] CORRADE_ALWAYS_INLINE uint32_t triple32_1(uint32_t x)
{
    return triple32<17,0xed5ad4bb,11,0xac4c1b51,15,0x31848bab,14>(x);
}

[[maybe_unused]] uint64_t nasam(uint64_t x) noexcept
{
    // NASAM by Pelle Evensen <https://mostlymangling.blogspot.com/2020/01/nasam-not-another-strange-acronym-mixer.html>

    x ^= std::rotr(x, 25) ^ std::rotr(x, 47);
    x *= 0x9E6C63D0676A9A99UL;
    x ^= x >> 23 ^ x >> 51;
    x *= 0x9E6D62D06F6A9A9BUL;
    x ^= x >> 23 ^ x >> 51;

    return x;
}

[[maybe_unused]] uint64_t splitmix64(uint64_t x)
{
    x = (x ^ (x >> 30)) * uint64_t(0xbf58476d1ce4e5b9ULL);
    x = (x ^ (x >> 27)) * uint64_t(0x94d049bb133111ebULL);
    x = x ^ (x >> 31);
    return x;
}


[[maybe_unused]] CORRADE_ALWAYS_INLINE uint32_t fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

[[maybe_unused]] uint32_t MurmurHash3_x86_32(const uint32_t* key, int len, uint32_t seed)
{
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len >> 2;
    int i;

    uint32_t h1 = seed;
    constexpr uint32_t c1 = 0xcc9e2d51;
    constexpr uint32_t c2 = 0x1b873593;

    const uint32_t* blocks = key + nblocks;

    for (i = -nblocks; i; i++)
    {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = std::rotl(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = std::rotl(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);

    uint32_t k1 = 0;

    switch (len & 3)
    {
    case 3:
        k1 ^= (uint32_t)(tail[2] << 16);
        [[fallthrough]];
    case 2:
        k1 ^= (uint32_t)(tail[1] << 8);
        [[fallthrough]];
    case 1:
        k1 ^= tail[0];
        k1 *= c1;
        k1 = std::rotl(k1, 15);
        k1 *= c2;
        h1 ^= k1;
    }

    h1 ^= (uint32_t)len;
    //h1 = hash32<16, 0x7feb352d, 15, 0x846ca68b, 16>(h1);
    h1 = fmix32(h1);

    return h1;
}

[[maybe_unused]] uint32_t murmur3(uint32_t x)
{
    return MurmurHash3_x86_32(&x, 4, 0xB16B00B5);
}

[[maybe_unused]] uint32_t murmur3_(uint64_t x)
{
    return MurmurHash3_x86_32((const uint32_t*)&x, 8, 0xB16B00B5);
}

[[maybe_unused]] constexpr auto std_hash = std::hash<uint64_t>{};

template<typename T>
constexpr bool is_prime(T n)
{
    // Corner cases
    if (n <= 1)  return false;
    if (n <= 3)  return true;

    if (n%2 == 0 || n%3 == 0)
        return false;

    for (T i = 5; i * i <= n; i = i + 6)
        if (n % i == 0 || n % (i + 2) == 0)
           return false;

    return true;
}

constexpr size_t next_prime(size_t pos)
{
    for (;;)
        if (is_prime(++pos))
           return pos;
    return pos;
}

template <typename F> [[maybe_unused]] void test_coords64(const char* name, F&& fun)
{
    constexpr int32_t max = 1 << 12;
    [[maybe_unused]] constexpr size_t iters = 4 * max * max;
    constexpr auto size = next_prime(iters * 2);
    //constexpr size_t size2 = 134217757;

    auto array = std::make_unique<uint32_t[]>(size);
    std::fill(array.get(), array.get() + size, 0);

    for (int32_t y = -max; y < max; y++)
        for (int32_t x = -max; x < max; x++)
        {
            global_coords coord{ x, y, 0 };
            const auto val = uint64_t{coord.y} << 32 | uint64_t{coord.x};
            const auto h = std::forward<F>(fun)(val) % size;
            ++array[h];
        }

    uint32_t count = 0, num = 0;

    for (unsigned i = 0; i < size; i++)
    {
        auto val = array[i];
        if (val > 1)
            num += val-1;
        count = std::max(val, count);
    }
    std::printf("%-16s max=%-10zu bad=%f%%\n", name, size_t{count}, (double)num * 100 / (double)size);
}

template<typename T, T offset_basis, T prime>
[[maybe_unused]] auto FNVHash(const char* str, size_t size)
{
    const auto* end = str + size;
    T hash = offset_basis;
    for (; str != end; ++str)
    {
        hash *= prime;
        hash ^= (uint8_t)*str;
    }
    return hash;
}

[[maybe_unused]] auto fnv1_(uint64_t x)
{
    return FNVHash<uint64_t, 0xcbf29ce484222325ULL, 0x100000001b3ULL>((char*)&x, 8);
}

} // namespace

#define FM_TEST_HASH_FUNCS

void test_app::test_hash()
{
#ifdef FM_TEST_HASH_FUNCS
    test_coords64("std::hash", [](uint64_t x) { return std_hash(x); });
    test_coords64("murmur3", [](uint64_t x) { return murmur3_(x); });
    test_coords64("fnv1", [](uint64_t x) { return fnv1_(x); });
    test_coords64("splitmix64", [](uint64_t x) { return (uint32_t)splitmix64(x); });
    test_coords64("nasam", [](uint64_t x) { return (uint32_t)nasam(x); });
#endif
}

} // namespace floormat
