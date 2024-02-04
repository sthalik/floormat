#include "src/raycast.hpp"
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

#pragma message("TODO!")

[[maybe_unused]] void Raycast(benchmark::State& state)
{
    for (auto _ : state)
        (void)0;
}

BENCHMARK(Raycast)->Unit(benchmark::kMicrosecond);

} // namespace

} // namespace floormat
