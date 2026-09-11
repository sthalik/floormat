#include "src/world.hpp"
#include "src/chunk.hpp"
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

constexpr chunk_coords_ coord_a{0, 0, 0};
constexpr chunk_coords_ coord_b{1, 0, 0};

void World_OperatorIndex_Hit(benchmark::State& state)
{
    auto w = world();
    w[coord_a];

    for (auto _ : state)
    {
        chunk* c = &w[coord_a];
        benchmark::DoNotOptimize(c);
    }
}
BENCHMARK(World_OperatorIndex_Hit)->Unit(benchmark::kNanosecond);

void World_OperatorIndex_Miss(benchmark::State& state)
{
    auto w = world();
    w[coord_a];
    w[coord_b];

    bool flip = false;
    for (auto _ : state)
    {
        chunk* c = &w[flip ? coord_b : coord_a];
        flip = !flip;
        benchmark::DoNotOptimize(c);
    }
}
BENCHMARK(World_OperatorIndex_Miss)->Unit(benchmark::kNanosecond);

void World_At(benchmark::State& state)
{
    auto w = world();
    w[coord_a];
    w[coord_b];

    bool flip = false;
    for (auto _ : state)
    {
        chunk* c = w.at(flip ? coord_b : coord_a);
        flip = !flip;
        benchmark::DoNotOptimize(c);
    }
}
BENCHMARK(World_At)->Unit(benchmark::kNanosecond);

} // namespace

} // namespace floormat
