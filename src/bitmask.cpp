#include "compat/exception.hpp"
#include "anim-atlas.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <cr/BitArray.h>
#include <cr/StridedArrayView.h>
#include <mg/ImageView.h>

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#endif
#include <hwy/highway.h>

namespace floormat {

using u8 = uint8_t;
using u32 = uint32_t;

namespace {

constexpr uint8_t amin = 32;

void bm_rows(const u8* __restrict src, u8* __restrict dest, u32 W, u32 H, u32 S)
{
    namespace hn = hwy::HWY_NAMESPACE;
    // A block's N bits and up to 7 carried ones fit in acc.
    constexpr hn::CappedTag<u8, 32> d8;
    constexpr hn::Repartition<int8_t, decltype(d8)> di8;
    constexpr hn::Repartition<int16_t, decltype(d8)> di16;
    constexpr hn::Repartition<int32_t, decltype(d8)> di32;
    constexpr auto N = (u32)hn::MaxLanes(d8);
    static_assert(amin >= 1 && amin <= 128);
    // Added to the alpha byte only. Its bit 7 becomes alpha >= amin.
    const auto bias = hn::BitCast(d8, hn::Set(di32, std::bit_cast<int32_t>(std::array<u8, 4>{0, 0, 0, 128 - amin})));
    // Alpha is the high byte of a 32-bit pixel on little-endian, the low one on big-endian.
    constexpr auto alpha_high = [](auto v) {
        if constexpr (std::endian::native == std::endian::big)
            return hn::ShiftLeft<24>(v);
        else
            return v;
    };

    for (auto j = 0u; j < H; j++)
    {
        const auto* row = src + (size_t)j * S;
        const auto bitʹ = (H - j - 1)*W;
        auto* p = dest + (bitʹ >> 3);
        uint64_t acc = 0;
        u32 have = bitʹ & 7, i = 0;

        if constexpr (N % 8 == 0)
            for (; i + N <= W; i += N)
            {
                const auto* q = row + (size_t)i*4;
                const auto v0 = alpha_high(hn::BitCast(di32, hn::SaturatedAdd(hn::LoadU(d8, q + 0*N), bias)));
                const auto v1 = alpha_high(hn::BitCast(di32, hn::SaturatedAdd(hn::LoadU(d8, q + 1*N), bias)));
                const auto v2 = alpha_high(hn::BitCast(di32, hn::SaturatedAdd(hn::LoadU(d8, q + 2*N), bias)));
                const auto v3 = alpha_high(hn::BitCast(di32, hn::SaturatedAdd(hn::LoadU(d8, q + 3*N), bias)));
                // Signed saturation keeps each lane's sign through both demotions.
                const auto v = hn::OrderedDemote2To(di8, hn::OrderedDemote2To(di16, v0, v1),
                                                         hn::OrderedDemote2To(di16, v2, v3));
                acc |= hn::BitsFromMask(di8, hn::Lt(v, hn::Zero(di8))) << have;
                // have is in [0,7] before every block, so each block fills exactly N/8 bytes.
                uint64_t w;
                std::memcpy(&w, p, N/8);
                // Byte k in memory takes bits 8k..8k+7 of acc.
                if constexpr (std::endian::native == std::endian::big)
                    w |= std::byteswap(acc);
                else
                    w |= acc;
                std::memcpy(p, &w, N/8);
                p += N/8;
                acc >>= N;
            }
        for (; i < W; i++)
        {
            acc |= (uint64_t)(row[(size_t)i*4 + 3] >= amin) << have;
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
