#include "compat/defs.hpp"
#include "compat/exception.hpp"
#include "anim-atlas.hpp"
#include <cstring>
#include <cr/BitArray.h>
#include <cr/StridedArrayView.h>
#include <mg/ImageView.h>

namespace floormat {

constexpr uint8_t amin = 32;

#if 1
using u8 = uint8_t;
using u32 = uint32_t;

namespace {

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

template<int N>
CORRADE_ALWAYS_INLINE
void bm_loop_body(const u8* __restrict src, u8* __restrict dest, u32 width, u32 height, u32 stride)
{
    for (auto j = 0u; j < height; j++)
    {
        auto i = 0u;
        while (i < (width & ~7u))
        {
            bm_loop<8>(src, dest, width, height, stride, i, j);
            i += 8;
        }
        if constexpr(N > 0)
        {
            bm_loop<N>(src, dest, width, height, stride, i, j);
            i += N;
        }
    }
}

} // namespace

void anim_atlas::make_bitmask_(const ImageView2D& tex, BitArray& bitmask)
{
    const auto pixels = tex.pixels();
    fm_soft_assert(tex.pixelSize() == 4);

    const auto* src   = (const u8*)pixels.data();
    auto* const dest  = (u8*)bitmask.data();
    const auto stride = (u32)pixels.stride()[0];
    const auto size   = pixels.size();
    const auto width  = (u32)size[1];
    const auto height = (u32)size[0];

    fm_debug_assert(bitmask.size() % 8 == 0);
    std::memset(bitmask.data(), 0, bitmask.size()/8);

    switch (width & 7)
    {
    default: std::unreachable();
    case 7: bm_loop_body<7>(src, dest, width, height, stride); break;
    case 6: bm_loop_body<6>(src, dest, width, height, stride); break;
    case 5: bm_loop_body<5>(src, dest, width, height, stride); break;
    case 4: bm_loop_body<4>(src, dest, width, height, stride); break;
    case 3: bm_loop_body<3>(src, dest, width, height, stride); break;
    case 2: bm_loop_body<2>(src, dest, width, height, stride); break;
    case 1: bm_loop_body<1>(src, dest, width, height, stride); break;
    case 0: bm_loop_body<0>(src, dest, width, height, stride); break;
    }
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
