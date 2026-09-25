#include "compat/assert.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include <cr/Algorithms.h>
#include <cr/Array.h>
#include <cr/StridedArrayView.h>
#include <mg/ImageData.h>
#include <mg/ImageView.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

void run(benchmark::State& state, const ImageView2D& img)
{
    auto bitmask = anim_atlas::make_bitmask(img);

    for (int i = 0; i < 3; i++)
        anim_atlas::make_bitmask_(img, bitmask);
    for (auto _ : state)
        anim_atlas::make_bitmask_(img, bitmask);
}

void Bitmask(benchmark::State& state)
{
    auto img = loader.texture(loader.SCENERY_PATH, "door-close"_s);
    run(state, img);
}

// With w % 4 == 0 every row starts 16-aligned.
void Bitmask_Width(benchmark::State& state)
{
    const auto img = loader.texture(loader.SCENERY_PATH, "door-close"_s);
    const auto w = (int)state.range(0), h = img.size().y();
    fm_assert(w <= img.size().x());

    Array<char> buf{NoInit, (size_t)w * (size_t)h * img.pixelSize()};
    fm_assert(((uintptr_t)buf.data() & 15) == 0);
    MutableImageView2D view{img.format(), {w, h}, buf};
    Utility::copy(img.pixels().prefix({(size_t)h, (size_t)w, img.pixelSize()}), view.pixels());
    run(state, view);
}

BENCHMARK(Bitmask)->Unit(benchmark::kMicrosecond);
BENCHMARK(Bitmask_Width)->ArgName("w")->Arg(3808)->Arg(3809)->Unit(benchmark::kMicrosecond);

} // namespace

} // namespace floormat
