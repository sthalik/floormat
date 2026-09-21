#include "compat/exception.hpp"
#include "anim-atlas.hpp"
#include <cstring>
#include <cr/BitArray.h>
#include <cr/StridedArrayView.h>
#include <mg/ImageView.h>
#ifdef __SSSE3__
#include <tmmintrin.h>
#endif

namespace floormat {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

namespace {

constexpr uint8_t amin = 32;

#ifdef __SSSE3__

void bm_rows(const u8* __restrict src, u8* __restrict dest, u32 W, u32 H, u32 S)
{
    const auto sel  = _mm_setr_epi8(3, 7, 11, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    const auto sign = _mm_set1_epi8((char)0x80);
    const auto thr  = _mm_set1_epi8((char)((amin - 1) ^ 0x80));

    for (auto j = 0u; j < H; j++)
    {
        const auto* row = src + (size_t)j * S;
        const auto bitʹ = (H - j - 1)*W;
        auto* p = dest + (bitʹ >> 3);
        u32 acc = 0, have = bitʹ & 7, i = 0;

        for (; i + 16 <= W; i += 16)
        {
            const auto* q = (const __m128i_u*)(row + (size_t)i*4);
            auto a0 = _mm_shuffle_epi8(_mm_loadu_si128(q + 0), sel);
            auto a1 = _mm_shuffle_epi8(_mm_loadu_si128(q + 1), sel);
            auto a2 = _mm_shuffle_epi8(_mm_loadu_si128(q + 2), sel);
            auto a3 = _mm_shuffle_epi8(_mm_loadu_si128(q + 3), sel);
            auto al = _mm_unpacklo_epi64(_mm_unpacklo_epi32(a0, a1), _mm_unpacklo_epi32(a2, a3));
            auto m = (u32)(u16)_mm_movemask_epi8(_mm_cmpgt_epi8(_mm_xor_si128(al, sign), thr));
            acc |= m << have;
            have += 16;
            do {
                *p++ |= (u8)acc;
                acc >>= 8;
                have -= 8;
            } while (have >= 8);
        }
        for (; i < W; i++)
        {
            acc |= (u32)(row[(size_t)i*4 + 3] >= amin) << have;
            if (++have >= 8)
            {
                *p++ |= (u8)acc;
                acc >>= 8;
                have -= 8;
            }
        }
        if (have)
            *p |= (u8)acc;
    }
}

#else

template<u32 Count>
CORRADE_ALWAYS_INLINE
void bm_loop(const u8* __restrict src, u8* __restrict dest, u32 W, u32 H, u32 S, u32 i, u32 j)
{
    auto bitʹ = (H - j - 1)*W + i;
    for (auto k = 0u; k < Count; k++)
    {
        bool value = src[(j * S + (i + k) * 4) + 3] >= amin;
        auto bit = bitʹ + k;
        auto& byte = dest[bit >> 3];
        byte |= u8{ value } << (bit & 7);
    }
}

void bm_rows(const u8* __restrict src, u8* __restrict dest, u32 W, u32 H, u32 S)
{
    const u32 head = W & ~7u;
    for (auto j = 0u; j < H; j++)
    {
        auto i = 0u;
        for (; i < head; i += 8)
            bm_loop<8>(src, dest, W, H, S, i, j);
        switch (W & 7)
        {
        case 7: bm_loop<1>(src, dest, W, H, S, i + 6, j); [[fallthrough]];
        case 6: bm_loop<1>(src, dest, W, H, S, i + 5, j); [[fallthrough]];
        case 5: bm_loop<1>(src, dest, W, H, S, i + 4, j); [[fallthrough]];
        case 4: bm_loop<1>(src, dest, W, H, S, i + 3, j); [[fallthrough]];
        case 3: bm_loop<1>(src, dest, W, H, S, i + 2, j); [[fallthrough]];
        case 2: bm_loop<1>(src, dest, W, H, S, i + 1, j); [[fallthrough]];
        case 1: bm_loop<1>(src, dest, W, H, S, i + 0, j); [[fallthrough]];
        case 0: break;
        default: std::unreachable();
        }
    }
}

#endif

} // namespace

#if 1
void anim_atlas::make_bitmask_(const ImageView2D& tex, BitArray& bitmask)
{
    const auto pixels = tex.pixels();
    fm_soft_assert(tex.pixelSize() == 4);
    fm_assert(bitmask.offset() == 0);

    const auto* src   = (const u8*)pixels.data();
    auto* const dest  = (u8*)bitmask.data();
    const auto stride = (u32)pixels.stride()[0];
    const auto size   = pixels.size();
    const auto width  = (u32)size[1];
    const auto height = (u32)size[0];

    fm_debug_assert(bitmask.size() % 8 == 0);
    std::memset(bitmask.data(), 0, bitmask.size()/8);

    bm_rows(src, dest, width, height, stride);
}
#else
void anim_atlas::make_bitmask_(const ImageView2D& tex, BitArray& bitmask)
{
    const auto pixels = tex.pixels();
    fm_soft_assert(tex.pixelSize() == 4);
    bitmask.resetAll();

    const auto* const src = (const unsigned char*)pixels.data();
    const auto stride = (size_t)pixels.stride()[0];
    const auto size   = pixels.size();
    const auto width  = size[1], height = size[0];

    for (auto j = 0u; j < height; j++)
        for (auto i = 0u; i < width; i++)
            bitmask.set((height - j - 1)*width + i, src[(j*stride + i*4)+3] >= amin);
}
#endif

} // namespace floormat
