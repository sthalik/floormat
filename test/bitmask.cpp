#include "app.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "compat/assert.hpp"
#include "compat/array-size.hpp"
#include <cr/Array.h>
#include <mg/Functions.h>
#include <mg/ImageData.h>
#include <mg/ImageView.h>
#include <mg/PixelFormat.h>

namespace floormat {

namespace {

const unsigned char src[] = {
#include "bitmask.embed.inc"
};

constexpr auto data_nbytes = array_size(src);
constexpr auto size = Vector2i{21, 52};
//static_assert(size_t{size.product()+7}/8 <= data_nbytes);

void bitmask_test()
{
    auto img = loader.texture("images/", "bitmask-test1"_s);
    auto bitmask = anim_atlas::make_bitmask(img);
    fm_assert(bitmask.size() >= size_t{size.product()});
    fm_assert(img.pixelSize() == 4);
//#define DO_GENERATE
#ifdef DO_GENERATE
    fputc('\n', stdout);
    for (auto i = 0u; i < (bitmask.size()+7)/8; i++)
    {
        printf("0x%02hhx,", bitmask.data()[i]);
        if (i % 15 == 14)
            printf("\n");
    }
    printf("\n");
    fflush(stdout);
#endif
    const auto len = Math::min(data_nbytes, (size_t)bitmask.size()+7 >> 3);
    fm_assert(array_size(src) >= len);
    for (auto i = 0uz; i < len; i++)
    {
        auto a = (unsigned char)bitmask.data()[i];
        if (a != src[i])
            fm_abort("wrong value 0x%02hhx at byte %zu, should be' 0x%02hhx'", a, i, src[i]);
    }
}

// Mirrors amin in src/bitmask.cpp, which has internal linkage there.
constexpr uint8_t amin = 32;

constexpr uint32_t pixel_hash(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// RGB holds the inverse of the alpha decision, so a shuffle mask reading the wrong byte of a
// pixel inverts every bit instead of passing.
void fill_pixels(unsigned char* buf, uint32_t W, uint32_t H, uint32_t seed)
{
    for (auto j = 0u; j < H; j++)
        for (auto i = 0u; i < W; i++)
        {
            const auto h = pixel_hash(seed*0x9e3779b9u + j*65537u + i);
            // Half the pixels straddle amin at 29..36, the rest spread over the whole range.
            const auto a = (uint8_t)(h & 1 ? h >> 24 : 29 + (h >> 8 & 7));
            auto* p = buf + (j*W + i)*4;
            p[0] = p[1] = p[2] = (uint8_t)(a >= amin ? 0x00 : 0xff);
            p[3] = a;
        }
}

void check_bitmask(const unsigned char* px, uint32_t W, uint32_t H)
{
    const ImageView2D img{PixelFormat::RGBA8Unorm, {(int)W, (int)H}, {px, W*H*4}};
    const auto bm = anim_atlas::make_bitmask(img);
    const auto nbits = (uint32_t)bm.size();
    fm_assert(nbits >= W*H);

    for (auto j = 0u; j < H; j++)
        for (auto i = 0u; i < W; i++)
        {
            const bool want = px[(j*W + i)*4 + 3] >= amin;
            const auto bit = (H - j - 1)*W + i;
            if (bm[bit] != want)
                fm_abort("bitmask %ux%u: bit %u at (%u,%u) is %d, should be %d",
                         W, H, bit, i, j, (int)bm[bit], (int)want);
        }

    // The allocation rounds up to a whole byte, so up to 7 bits of overrun land inside it where
    // ASan cannot see them.
    for (auto bit = W*H; bit < nbits; bit++)
        if (bm[bit])
            fm_abort("bitmask %ux%u: bit %u past the image is set", W, H, bit);
}

void bitmask_sweep_test()
{
    // Width covers every residue mod 16 three times, with zero, one and two whole SSE2 blocks
    // ahead of the scalar tail. Height covers every start offset: row j begins at bit
    // (H-j-1)*W, so eight rows walk the full cycle of W mod 8.
    constexpr uint32_t max_w = 48, max_h = 9;
    unsigned char px[max_w*max_h*4];

    for (auto W = 1u; W <= max_w; W++)
        for (auto H = 1u; H <= max_h; H++)
        {
            fill_pixels(px, W, H, W*(max_h+1) + H);
            check_bitmask(px, W, H);
        }
}

void bitmask_wide_test()
{
    // The sweep stops at 48 where anim/npc-walk.png is 3382 wide. 3391 is prime, runs 211 SSE2
    // blocks with the widest possible tail, and 3391 % 8 == 7 walks every byte offset.
    constexpr uint32_t W = 3391, H = 64;
    Array<unsigned char> px{NoInit, (size_t)W*H*4};
    fill_pixels(px.data(), W, H, 0xa5f3u);
    check_bitmask(px.data(), W, H);
}

} // namespace

void Test::test_bitmask()
{
    bitmask_sweep_test();
    bitmask_wide_test();
    bitmask_test();
    //bitmask_benchmark();
}

} // namespace floormat
