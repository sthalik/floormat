#include "src/world.hpp"
#include "src/chunk.hpp"
#include <cr/GrowableArray.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

void make_chunks(world& w, i16 side)
{
    for (i16 y = 0; y < side; y++)
        for (i16 x = 0; x < side; x++)
            w[chunk_coords_{x, y, 0}];
}

void World_Create(benchmark::State& state)
{
    const auto side = (i16)state.range(0);
    {
        auto w = world();
        make_chunks(w, side);
    }

    for (auto _ : state)
    {
        auto w = world();
        make_chunks(w, side);
        benchmark::DoNotOptimize(&w);
    }
}
BENCHMARK(World_Create)->ArgName("side")->Arg(0)->Arg(2)->Arg(8)->Unit(benchmark::kMicrosecond);

void World_Create_Keep(benchmark::State& state)
{
    const auto side = (i16)state.range(0);
    {
        auto w = world();
        make_chunks(w, side);
    }
    Array<world> worlds;
    arrayReserve(worlds, (usize)state.max_iterations);

    for (auto _ : state)
        make_chunks(arrayAppend(worlds, InPlaceInit), side);
}
// Every world holds a 2 MiB large-page chunk table top. One failed large-page allocation
// switches the whole process to 4 KiB pages, so the iteration count stays fixed.
BENCHMARK(World_Create_Keep)->ArgName("side")->Arg(0)->Arg(2)->Arg(8)->Iterations(32)->Unit(benchmark::kMicrosecond);

} // namespace

} // namespace floormat
