#include "compat/assert.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include <Corrade/Containers/Optional.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Trade/ImageData.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

[[maybe_unused]] void Bitmask(benchmark::State& state)
{
    auto img = loader.texture(loader.SCENERY_PATH, "door-close"_s);
    auto bitmask = anim_atlas::make_bitmask(img);
    anim_atlas::make_bitmask_(img, bitmask);

    for (auto _ : state)
        anim_atlas::make_bitmask_(img, bitmask);
}

BENCHMARK(Bitmask)->Unit(benchmark::kMicrosecond)->ReportAggregatesOnly();

} // namespace

} // namespace floormat
