#include "compat/exception.hpp"
#include "anim-atlas.hpp"
#include <cstring>
#include <cr/BitArray.h>
#include <cr/StridedArrayView.h>
#include <mg/ImageView.h>
// MSVC never defines __SSE2__.
#if defined __SSE2__ || defined _M_X64 || (defined _M_IX86_FP && _M_IX86_FP >= 2)
#define FM_BITMASK_SSE2
#include <emmintrin.h>
#endif

namespace floormat {

using u8 = uint8_t;
using u32 = uint32_t;

namespace {

constexpr uint8_t amin = 32;

#ifdef FM_BITMASK_SSE2

void bm_rows(const u8* __restrict src, u8* __restrict dest, u32 W, u32 H, u32 S)
{
    static_assert(amin >= 1 && amin <= 128);
    // Added to the alpha byte only. Bit 31 of each pixel becomes alpha >= amin.
    const auto bias = _mm_set1_epi32((int)((128u - amin) << 24));

    for (auto j = 0u; j < H; j++)
    {
        const auto* row = src + (size_t)j * S;
        const auto bitʹ = (H - j - 1)*W;
        auto* p = dest + (bitʹ >> 3);
        u32 acc = 0, have = bitʹ & 7, i = 0;

        for (; i + 16 <= W; i += 16)
        {
            // MSVC has no __m128i_u, and -Wcast-align rejects a direct cast from u8*.
            const auto* q = (const __m128i*)(const void*)(row + (size_t)i*4);
            auto v0 = _mm_adds_epu8(_mm_loadu_si128(q + 0), bias);
            auto v1 = _mm_adds_epu8(_mm_loadu_si128(q + 1), bias);
            auto v2 = _mm_adds_epu8(_mm_loadu_si128(q + 2), bias);
            auto v3 = _mm_adds_epu8(_mm_loadu_si128(q + 3), bias);
            // Signed saturation keeps each lane's sign through both packs.
            auto m = (u32)_mm_movemask_epi8(_mm_packs_epi16(_mm_packs_epi32(v0, v1), _mm_packs_epi32(v2, v3)));
            acc |= m << have;
            // have is in [0,7] before every block, so each block fills exactly two bytes.
            uint16_t w;
            std::memcpy(&w, p, sizeof w);
            w |= (uint16_t)acc;
            std::memcpy(p, &w, sizeof w);
            p += 2;
            acc >>= 16;
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
