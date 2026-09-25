#include "src/world.hpp"
#include "src/chunk.hpp"
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

void World_Create(benchmark::State& state)
{
    const auto side = (int16_t)state.range(0);

    for (auto _ : state)
    {
        auto w = world();
        for (int16_t y = 0; y < side; y++)
            for (int16_t x = 0; x < side; x++)
                w[chunk_coords_{x, y, 0}];
        benchmark::DoNotOptimize(&w);
    }
}
BENCHMARK(World_Create)->ArgName("side")->Arg(0)->Arg(2)->Arg(8)->Unit(benchmark::kMicrosecond);

} // namespace

} // namespace floormat
