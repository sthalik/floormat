#include "murmur.hpp"

static MURMUR_FORCE_INLINE uint32_t getblock32(const std::uint32_t* p, int i)
{
    return p[i];
}

#ifdef _MSC_VER
static MURMUR_FORCE_INLINE std::uint32_t rotl32(std::uint32_t x, int n)
{
    return _rotl(x, n);
}
#else
static MURMUR_FORCE_INLINE std::uint32_t rotl32(std::uint32_t x, int n)
{
    return ((x << n) | (x >> (64 - n)));
}
#endif

static std::uint32_t fmix32(std::uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

murmur_result murmur128(const void* key, int len, std::uint32_t seed)
{
    using uint64_t = std::uint64_t;
    using uint8_t = std::uint8_t;
    using uint32_t = std::uint32_t;

    const uint8_t* __restrict data = reinterpret_cast<const uint8_t*>(key);
    const int nblocks = len / 16;

    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;

    static constexpr uint32_t c1 = 0x239b961b;
    static constexpr uint32_t c2 = 0xab0e9789;
    static constexpr uint32_t c3 = 0x38b34ae5;
    static constexpr uint32_t c4 = 0xa1e38b93;

    uint32_t const* const __restrict blocks = reinterpret_cast<const uint32_t*>(data + nblocks*16);

    for (int i = -nblocks; i; i++)
    {
        uint32_t k1 = getblock32(blocks,i*4+0);
        uint32_t k2 = getblock32(blocks,i*4+1);
        uint32_t k3 = getblock32(blocks,i*4+2);
        uint32_t k4 = getblock32(blocks,i*4+3);

        k1 *= c1; k1  = rotl32(k1,15); k1 *= c2; h1 ^= k1;

        h1 = rotl32(h1,19); h1 += h2; h1 = h1*5+0x561ccd1b;

        k2 *= c2; k2  = rotl32(k2,16); k2 *= c3; h2 ^= k2;

        h2 = rotl32(h2,17); h2 += h3; h2 = h2*5+0x0bcaa747;

        k3 *= c3; k3  = rotl32(k3,17); k3 *= c4; h3 ^= k3;

        h3 = rotl32(h3,15); h3 += h4; h3 = h3*5+0x96cd1c35;

        k4 *= c4; k4  = rotl32(k4,18); k4 *= c1; h4 ^= k4;

        h4 = rotl32(h4,13); h4 += h1; h4 = h4*5+0x32ac3b17;
    }

    uint8_t const* const __restrict tail = reinterpret_cast<const uint8_t*>(data + nblocks*16);

    uint32_t k1 = 0;
    uint32_t k2 = 0;
    uint32_t k3 = 0;
    uint32_t k4 = 0;

    switch(len & 15)
    {
    case 15: k4 ^= tail[14] << 16;
    case 14: k4 ^= tail[13] << 8;
    case 13: k4 ^= tail[12] << 0;
        k4 *= c4; k4  = rotl32(k4,18); k4 *= c1; h4 ^= k4;

    case 12: k3 ^= tail[11] << 24;
    case 11: k3 ^= tail[10] << 16;
    case 10: k3 ^= tail[ 9] << 8;
    case  9: k3 ^= tail[ 8] << 0;
        k3 *= c3; k3  = rotl32(k3,17); k3 *= c4; h3 ^= k3;

    case  8: k2 ^= tail[ 7] << 24;
    case  7: k2 ^= tail[ 6] << 16;
    case  6: k2 ^= tail[ 5] << 8;
    case  5: k2 ^= tail[ 4] << 0;
        k2 *= c2; k2  = rotl32(k2,16); k2 *= c3; h2 ^= k2;

    case  4: k1 ^= tail[ 3] << 24;
    case  3: k1 ^= tail[ 2] << 16;
    case  2: k1 ^= tail[ 1] << 8;
    case  1: k1 ^= tail[ 0] << 0;
        k1 *= c1; k1  = rotl32(k1,15); k1 *= c2; h1 ^= k1;
    };

    // finalization

    h1 ^= len; h2 ^= len; h3 ^= len; h4 ^= len;

    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;

    h1 = fmix32(h1);
    h2 = fmix32(h2);
    h3 = fmix32(h3);
    h4 = fmix32(h4);

    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;

    return murmur_result(uint64_t(h1) << 32 | uint64_t(h2) << 0,
                         uint64_t(h3) << 32 | uint64_t(h4) << 0);
}
