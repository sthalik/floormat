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

CORRADE_ALWAYS_INLINE void U32TO8_LE(u8* __restrict p, u32 v)
{
    (p)[0] = (u8)((v));
    (p)[1] = (u8)((v) >> 8);
    (p)[2] = (u8)((v) >> 16);
    (p)[3] = (u8)((v) >> 24);
}

CORRADE_ALWAYS_INLINE void U64TO8_LE(u8* __restrict p, u64 v)
{
    U32TO8_LE((p), (u32)((v)));
    U32TO8_LE((p) + 4, (u32)((v) >> 32));
}

constexpr CORRADE_ALWAYS_INLINE u64 U8TO64_LE(const u8* __restrict p)
{
    return (((u64)((p)[0])) | ((u64)((p)[1]) << 8) |
            ((u64)((p)[2]) << 16) | ((u64)((p)[3]) << 24) |
            ((u64)((p)[4]) << 32) | ((u64)((p)[5]) << 40) |
            ((u64)((p)[6]) << 48) | ((u64)((p)[7]) << 56));
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

struct Key { u64 k0, k1; };
constexpr Key make_key()
{
    u8 buf[16];
    for (u8 i = 0; i < 16; i++)
        buf[i] = i;
    return { U8TO64_LE(buf), U8TO64_LE(buf+8) };
}

/*
    Computes a SipHash value
    *in: pointer to input data (read-only)
    inlen: input data length in bytes (any usize value)
    *k: pointer to the key data (read-only), must be 16 bytes
    *out: pointer to output data (write-only), outlen bytes must be allocated
    outlen: length of the output in bytes, must be 8 or 16
*/
void siphash(const void * __restrict in, const usize inlen, u8 * __restrict out) {

    const auto* ni = (const unsigned char *)in;

    u64 v0 = UINT64_C(0x736f6d6570736575);
    u64 v1 = UINT64_C(0x646f72616e646f6d);
    u64 v2 = UINT64_C(0x6c7967656e657261);
    u64 v3 = UINT64_C(0x7465646279746573);
    //u64 k0 = U8TO64_LE(kk);
    //u64 k1 = U8TO64_LE(kk + 8);
    constexpr auto key_ = make_key();
    constexpr u64 k0 = key_.k0;
    constexpr u64 k1 = key_.k1;
    u64 m;
    int i;
    const u8 *end = ni + inlen - (inlen % sizeof(u64));
    const auto left = (u32)(inlen & 7);
    u64 b = ((u64)inlen) << 56;
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
        b |= ((u64)ni[6]) << 48; [[fallthrough]];
    case 6:
        b |= ((u64)ni[5]) << 40; [[fallthrough]];
    case 5:
        b |= ((u64)ni[4]) << 32; [[fallthrough]];
    case 4:
        b |= ((u64)ni[3]) << 24; [[fallthrough]];
    case 3:
        b |= ((u64)ni[2]) << 16; [[fallthrough]];
    case 2:
        b |= ((u64)ni[1]) << 8;  [[fallthrough]];
    case 1:
        b |= ((u64)ni[0]);       [[fallthrough]];
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

usize hash_buf(const void* __restrict buf, usize size) noexcept
{
    u64 ret;
    siphash(buf, size, reinterpret_cast<u8*>(&ret));
    return (usize)ret;
}

usize hash_int(u32 x) noexcept
{
    return hash_buf(&x, sizeof x);
}

usize hash_int(u64 x) noexcept
{
    return hash_buf(&x, sizeof x);
}

} // namespace floormat::SipHash

// NOLINTEND(*-switch-missing-default-case, *-multiway-paths-covered)
