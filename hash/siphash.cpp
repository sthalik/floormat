#include "hash-impl.hpp"
#include <bit>

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

// ReSharper disable CppDefaultCaseNotHandledInSwitchStatement
// NOLINTBEGIN(*-switch-missing-default-case, *-multiway-paths-covered)

namespace floormat::SipHash {
namespace {

/* default: SipHash-2-4 */
constexpr inline int cROUNDS = 2, dROUNDS = 4;

CORRADE_ALWAYS_INLINE void U32TO8_LE(uint8_t* __restrict p, uint32_t v)
{
    (p)[0] = (uint8_t)((v));
    (p)[1] = (uint8_t)((v) >> 8);
    (p)[2] = (uint8_t)((v) >> 16);
    (p)[3] = (uint8_t)((v) >> 24);
}

CORRADE_ALWAYS_INLINE void U64TO8_LE(uint8_t* __restrict p, uint64_t v)
{
    U32TO8_LE((p), (uint32_t)((v)));
    U32TO8_LE((p) + 4, (uint32_t)((v) >> 32));
}

constexpr CORRADE_ALWAYS_INLINE uint64_t U8TO64_LE(const uint8_t* __restrict p)
{
    return (((uint64_t)((p)[0])) | ((uint64_t)((p)[1]) << 8) |
            ((uint64_t)((p)[2]) << 16) | ((uint64_t)((p)[3]) << 24) |
            ((uint64_t)((p)[4]) << 32) | ((uint64_t)((p)[5]) << 40) |
            ((uint64_t)((p)[6]) << 48) | ((uint64_t)((p)[7]) << 56));
}

#define SIPROUND                                                               \
    do {                                                                       \
        v0 += v1;                                                              \
        v1 = std::rotl(v1, 13);                                                \
        v1 ^= v0;                                                              \
        v0 = std::rotl(v0, 32);                                                \
        v2 += v3;                                                              \
        v3 = std::rotl(v3, 16);                                                \
        v3 ^= v2;                                                              \
        v0 += v3;                                                              \
        v3 = std::rotl(v3, 21);                                                \
        v3 ^= v0;                                                              \
        v2 += v1;                                                              \
        v1 = std::rotl(v1, 17);                                                \
        v1 ^= v2;                                                              \
        v2 = std::rotl(v2, 32);                                                \
    } while (0)

struct Key { uint64_t k0, k1; };
constexpr Key make_key()
{
    uint8_t buf[16];
    for (uint8_t i = 0; i < 16; i++)
        buf[i] = i;
    return { U8TO64_LE(buf), U8TO64_LE(buf+8) };
}

/*
    Computes a SipHash value
    *in: pointer to input data (read-only)
    inlen: input data length in bytes (any size_t value)
    *k: pointer to the key data (read-only), must be 16 bytes
    *out: pointer to output data (write-only), outlen bytes must be allocated
    outlen: length of the output in bytes, must be 8 or 16
*/
void siphash(const void * __restrict in, const size_t inlen, uint8_t * __restrict out) {

    const auto* ni = (const unsigned char *)in;

    uint64_t v0 = UINT64_C(0x736f6d6570736575);
    uint64_t v1 = UINT64_C(0x646f72616e646f6d);
    uint64_t v2 = UINT64_C(0x6c7967656e657261);
    uint64_t v3 = UINT64_C(0x7465646279746573);
    //uint64_t k0 = U8TO64_LE(kk);
    //uint64_t k1 = U8TO64_LE(kk + 8);
    constexpr auto key_ = make_key();
    constexpr uint64_t k0 = key_.k0;
    constexpr uint64_t k1 = key_.k1;
    uint64_t m;
    int i;
    const uint8_t *end = ni + inlen - (inlen % sizeof(uint64_t));
    const auto left = (uint32_t)(inlen & 7);
    uint64_t b = ((uint64_t)inlen) << 56;
    v3 ^= k1;
    v2 ^= k0;
    v1 ^= k1;
    v0 ^= k0;

#if 0
    if (outlen == 16)
        v1 ^= 0xee;
#endif

    for (; ni != end; ni += 8) {
        m = U8TO64_LE(ni);
        v3 ^= m;

        for (i = 0; i < cROUNDS; ++i)
            SIPROUND;

        v0 ^= m;
    }

    switch (left) {
    case 7:
        b |= ((uint64_t)ni[6]) << 48; [[fallthrough]];
    case 6:
        b |= ((uint64_t)ni[5]) << 40; [[fallthrough]];
    case 5:
        b |= ((uint64_t)ni[4]) << 32; [[fallthrough]];
    case 4:
        b |= ((uint64_t)ni[3]) << 24; [[fallthrough]];
    case 3:
        b |= ((uint64_t)ni[2]) << 16; [[fallthrough]];
    case 2:
        b |= ((uint64_t)ni[1]) << 8;  [[fallthrough]];
    case 1:
        b |= ((uint64_t)ni[0]);       [[fallthrough]];
    case 0:
        break;
    }

    v3 ^= b;

    for (i = 0; i < cROUNDS; ++i)
        SIPROUND;

    v0 ^= b;

#if 0
    if (outlen == 16)
        v2 ^= 0xee;
    else
#endif
        v2 ^= 0xff;

    for (i = 0; i < dROUNDS; ++i)
        SIPROUND;

    b = v0 ^ v1 ^ v2 ^ v3;
    U64TO8_LE(out, b);

#if 0
    if (outlen == 8)
        return 0;

    v1 ^= 0xdd;

    TRACE;
    for (i = 0; i < dROUNDS; ++i)
        SIPROUND;

    b = v0 ^ v1 ^ v2 ^ v3;
    U64TO8_LE(out + 8, b);
#endif
}

} // namespace

size_t hash_buf(const void* __restrict buf, size_t size) noexcept
{
    uint64_t ret;
    siphash(buf, size, reinterpret_cast<uint8_t*>(&ret));
    return (size_t)ret;
}

size_t hash_int(uint32_t x) noexcept
{
    return hash_buf(&x, sizeof x);
}

size_t hash_int(uint64_t x) noexcept
{
    return hash_buf(&x, sizeof x);
}

} // namespace floormat::SipHash

// NOLINTEND(*-switch-missing-default-case, *-multiway-paths-covered)
