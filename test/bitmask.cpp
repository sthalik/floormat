#include "app.hpp"
#include "compat/assert.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include <iterator>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <chrono>

namespace floormat {

namespace {

constexpr Vector4ub
    red(0xff, 0x00, 0x00, 0x20), green(0x00, 0x00, 0xff, 0x20), blue(0xff, 0x00, 0x00, 0x20),
    red0(0xff, 0x00, 0x00, 0x1f), green0(0x00, 0x00, 0xff, 0x1f), blue0(0xff, 0x00, 0x00, 0x1f),
    none0(0xff, 0xff, 0xff, 0x1f), white(0xff, 0xff, 0xff, 0xff);

constexpr Vector4ub img_data[] = {
    red,   green0, blue0, white,
    red0,  green,  blue,  none0,
    none0, green,  blue0, blue,
    red0,  green0, blue,  green,
    red,   green,  blue,  none0,
    red,   green0, blue0, red,
};

constexpr bool result[] = {
    // inverse row order, use tac(1)
    true,  false, false, true,
    true,  true,  true,  false,
    false, false, true,  true,
    false, true,  false, true,
    false, true,  true,  false,
    true,  false, false, true,

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
    constexpr auto size = std::size(img_data), width = 4_uz, height = size/4;
    static_assert(size % 4 == 0);
    static_assert(size == std::size(result));
    ImageView2D img{PixelFormat::RGBA8Unorm, {width, height}, img_data};
    auto bitmask = anim_atlas::make_bitmask(img);
    fm_assert(img.size().product() == std::size(result));
    for (auto i = 0_uz; i < size; i++)
        if (bitmask[i] != result[i])
            fm_abort("wrong value at bit %zu, should be %s", i, result[i] ? "true" : "false");
}

} // namespace

void test_app::test_bitmask()
{
    bitmask_test();
    //bitmask_benchmark();
}

} // namespace floormat
