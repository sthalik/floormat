#include "chacha.hpp"
#include "os-seed.hpp"
#include "compat/array-size.hpp"
#include "compat/iota.hpp"
#include <bit>
#include <cstring>
#include <cr/Algorithms.h>
#include <cr/ArrayView.h>
#include <mg/Functions.h>

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#endif
#undef HWY_DISABLED_TARGETS
#define HWY_DISABLED_TARGETS (HWY_AVX3_ZEN4 | HWY_AVX3_DL | HWY_AVX3_SPR | HWY_AVX10_2 | HWY_ALL_SVE | HWY_RVV)
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "random/chacha.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#ifndef FLOORMAT_RANDOM_CHACHA_CPP_SHARED
#define FLOORMAT_RANDOM_CHACHA_CPP_SHARED
namespace floormat::Random::ChaCha {

namespace {

constexpr u32 sigma[4] = { 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574 };
constexpr u32 blocks_per_call = 32;

} // namespace

} // namespace floormat::Random::ChaCha
#endif

HWY_BEFORE_NAMESPACE();
#if defined __GNUC__ && !defined __clang__ && defined _WIN32 && HWY_ARCH_X86_64
#pragma GCC optimize("no-asynchronous-unwind-tables", "no-unwind-tables")
#endif
namespace floormat::Random::ChaCha::HWY_NAMESPACE {
namespace {

namespace hn = hwy::HWY_NAMESPACE;

constexpr hn::CappedTag<u32, 16> du32;
constexpr hn::Repartition<u64, decltype(du32)> du64;
constexpr auto lanes = (u32)hn::MaxLanes(du32);
constexpr u32 blocks_per_lane = HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3 ? 2 : 1;
static_assert(lanes % 4 == 0 && blocks_per_call % (blocks_per_lane * lanes) == 0);
using V = hn::Vec<decltype(du32)>;

template<int Bits> HWY_INLINE V rotl(V v)
{
#if HWY_ARCH_X86 && HWY_TARGET > HWY_AVX3 && HWY_TARGET <= HWY_SSSE3
    if constexpr (Bits == 8 || Bits == 16)
    {
        constexpr hn::Repartition<u8, decltype(du32)> du8;
        auto index = Bits == 16
            ? hn::Dup128VecFromValues(du8, 2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13)
            : hn::Dup128VecFromValues(du8, 3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14);
#ifdef __clang__
        // Hides the index from clang, which otherwise emits pshuflw and pshufhw for rotl<16>
        // and two pshufb for rotl<8>(d ^ a).
        asm("" : "+x"(index.raw));
#endif
        return hn::BitCast(du32, hn::TableLookupBytes(hn::BitCast(du8, v), index));
    }
    else
        return hn::RotateLeft<Bits>(v);
#else
    return hn::RotateLeft<Bits>(v);
#endif
}

HWY_INLINE void quarter_round(V& a, V& b, V& c, V& d)
{
    a = hn::Add(a, b);
    d = rotl<16>(hn::Xor(d, a));
    c = hn::Add(c, d);
    b = rotl<12>(hn::Xor(b, c));
    a = hn::Add(a, b);
    d = rotl<8>(hn::Xor(d, a));
    c = hn::Add(c, d);
    b = rotl<7>(hn::Xor(b, c));
}

HWY_INLINE void quarter_rounds(V (&x)[blocks_per_lane][16], u32 a, u32 b, u32 c, u32 d)
{
    for (u32 i = 0; i < blocks_per_lane; i++)
        quarter_round(x[i][a], x[i][b], x[i][c], x[i][d]);
}

HWY_INLINE void init(V (&x)[16], const u32* __restrict seed, u64 block, u64 stream)
{
    for (u32 i = 0; i < 4; i++)
        x[i] = hn::Set(du32, sigma[i]);
    for (u32 i = 0; i < 8; i++)
        x[4 + i] = hn::Set(du32, seed[i]);
    // hn::Iota yields lanes in reverse order on GCC aarch64_be NEON.
    x[12] = hn::Add(hn::Set(du32, (u32)block), hn::LoadU(du32, iota_array<u32, lanes>.data()));
    x[13] = hn::Set(du32, (u32)(block >> 32));
    x[14] = hn::Set(du32, (u32)stream);
    x[15] = hn::Set(du32, (u32)(stream >> 32));
}

template<u32 Chunk = 0> HWY_INLINE void store_output(V v, u64* __restrict p)
{
    if constexpr (std::endian::native == std::endian::big)
    {
        // BitCast to u64 keeps register bits on GCC aarch64_be NEON, and the memory image on Z and PPC.
        u32 words[lanes];
        hn::StoreU(hn::Reverse2(du32, v), du32, words);
        for (u32 i = 0; i < lanes / 4; i++)
            std::memcpy(p + i * 32, words + i * 4, 4 * sizeof(u32));
    }
    else if constexpr (lanes == 4)
        hn::StoreU(hn::BitCast(du64, v), du64, p);
    else
    {
        constexpr hn::Full128<u64> d128;
        hn::StoreU(hn::ExtractBlock<(int)Chunk>(hn::BitCast(du64, v)), d128, p + Chunk * 32);
        if constexpr (Chunk + 1 < lanes / 4)
            store_output<Chunk + 1>(v, p);
    }
}

#if defined __clang__ && HWY_TARGET == HWY_AVX2
[[gnu::force_align_arg_pointer]]
#endif
void generate(const u32* __restrict seed, u64 counter, u64 stream, u64* __restrict out)
{
    for (u32 first = 0; first < blocks_per_call; first += blocks_per_lane * lanes)
    {
        V x[blocks_per_lane][16];
        HWY_UNROLL(blocks_per_lane)
        for (u32 b = 0; b < blocks_per_lane; b++)
            init(x[b], seed, counter + first + b * lanes, stream);
        for (u32 i = 0; i < rounds; i += 2)
        {
            quarter_rounds(x, 0, 4, 8, 12);
            quarter_rounds(x, 1, 5, 9, 13);
            quarter_rounds(x, 2, 6, 10, 14);
            quarter_rounds(x, 3, 7, 11, 15);
            quarter_rounds(x, 0, 5, 10, 15);
            quarter_rounds(x, 1, 6, 11, 12);
            quarter_rounds(x, 2, 7, 8, 13);
            quarter_rounds(x, 3, 4, 9, 14);
        }
        HWY_UNROLL(blocks_per_lane)
        for (u32 b = 0; b < blocks_per_lane; b++)
        {
            for (u32 i = 0; i < 8; i++)
                x[b][4 + i] = hn::Add(x[b][4 + i], hn::Set(du32, seed[i]));
            for (u32 w = 0; w < 16; w++)
                store_output(x[b][w], out + (first + b * lanes) * 8 + w * 2);
        }
    }
}

} // namespace
} // namespace floormat::Random::ChaCha::HWY_NAMESPACE
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace floormat::Random::ChaCha {

HWY_EXPORT(generate);

csprng::csprng() :
    stream{0}, counter{0}, index{(u32)static_array_size<decltype(buffer)>}
{
    u8 bytes[sizeof seed];
    os_random_bytes(bytes);
    std::memcpy(seed, bytes, sizeof bytes);
}

csprng::csprng(const u8 (&seed)[32], u64 stream) :
    stream{stream}, counter{0}, index{(u32)static_array_size<decltype(buffer)>}
{
    for (u32 i = 0; i < 8; i++)
        this->seed[i] = (u32)seed[4*i + 0] <<  0 | (u32)seed[4*i + 1] <<  8
                     | (u32)seed[4*i + 2] << 16 | (u32)seed[4*i + 3] << 24;
}

void csprng::refill()
{
    static_assert(static_array_size<decltype(buffer)> == blocks_per_call * 8);
    HWY_DYNAMIC_DISPATCH(generate)(seed, counter, stream, buffer);
    counter += blocks_per_call;
    index = 0;
}

u64 csprng::next()
{
    if (index == static_array_size<decltype(buffer)>) [[unlikely]]
        refill();
    return buffer[index++];
}

void csprng::fill(u64* out, u32 n)
{
    constexpr auto size = (u32)static_array_size<decltype(buffer)>;
    const u32 buffered = Math::min(n, size - index);
    Utility::copy(arrayView(buffer + index, buffered), arrayView(out, buffered));
    index += buffered;
    out += buffered;
    n -= buffered;
    for (; n >= size; n -= size, out += size)
    {
        HWY_DYNAMIC_DISPATCH(generate)(seed, counter, stream, out);
        counter += blocks_per_call;
    }
    if (n > 0)
    {
        refill();
        Utility::copy(arrayView(buffer, n), arrayView(out, n));
        index = n;
    }
}

void csprng::discard(u64 n)
{
    constexpr auto size = (u32)static_array_size<decltype(buffer)>;
    const u32 buffered = size - index;
    if (n <= buffered)
        index += (u32)n;
    else
    {
        n -= buffered;
        counter += n / size * blocks_per_call;
        index = size;
        if (n % size > 0)
        {
            refill();
            index = (u32)(n % size);
        }
    }
}

} // namespace floormat::Random::ChaCha
#endif
