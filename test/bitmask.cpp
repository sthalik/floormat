#include "app.hpp"
#include "compat/assert.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include <iterator>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <chrono>

//#define DO_GENERATE

namespace floormat {

namespace {

constexpr size_t data_nbytes = 128;
const unsigned char data_door_close[] = {
#include "bitmask.embed.inc"
};

[[maybe_unused]] void bitmask_benchmark()
{
    std::chrono::high_resolution_clock clock;

    auto img = loader.texture(loader.SCENERY_PATH, "door-close"_s);
    auto bitmask = anim_atlas::make_bitmask(img);
    constexpr int runs = 10, warmup = 500, cycles = 1000;
    for (int i = 0; i < warmup; i++)
        anim_atlas::make_bitmask_(img, bitmask);
    for (int i = 0; i < runs; i++)
    {
        auto time0 = clock.now();
        for (int i = 0; i < cycles; i++)
            (void)anim_atlas::make_bitmask_(img, bitmask);
        std::chrono::duration<double, std::milli> time = clock.now() - time0;

        fm_log("[BENCH] bitmask %d/%d took %.1f ms", i, runs, time.count());
    }
}

void bitmask_test()
{
    auto img = loader.texture(loader.SCENERY_PATH, "door-close"_s);
    auto bitmask = anim_atlas::make_bitmask(img);
    fm_assert(img.pixelSize() == 4 && (size_t)img.size().product() >= data_nbytes);
#ifdef DO_GENERATE
    for (auto i = 0uz; i < data_nbytes; i++)
    {
        if (i && i % 16 == 0)
            printf("\n");
        printf("0x%02hhx,", bitmask.data()[i]);
        if ((i+1) % 16 != 0)
            printf(" ");
    }
    printf("\n");
    fflush(stdout);
#endif
    const auto len = std::min(data_nbytes, (size_t)bitmask.size()+7 >> 3);
    for (auto i = 0uz; i < len; i++)
        if ((unsigned char)bitmask.data()[i] != data_door_close[i])
            fm_abort("wrong value at bit %zu, should be' 0x%02hhx'", i, data_door_close[i]);
}

} // namespace

void test_app::test_bitmask()
{
    bitmask_test();
    //bitmask_benchmark();
}

} // namespace floormat
