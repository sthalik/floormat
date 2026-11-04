#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/light.hpp"
#include "src/tile-defs.hpp"
#include "compat/borrowed-ptr.inl"
#include <cr/Array.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

constexpr chunk_coords_ coord_a{0, 0, 0};
constexpr chunk_coords_ coord_b{1, 0, 0};

constexpr i16 find_side = 17;
constexpr u32 find_per_chunk = 448;

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

void World_FindObject(benchmark::State& state)
{
    const auto num_z = (i32)state.range(0);
    const bool shuffled = state.range(1) != 0;
    const auto num_objects = u32(find_side * find_side * num_z) * find_per_chunk;

    auto w = world();
    Array<object_id> ids{NoInit, num_objects};
    light_proto p;
    // A zero bbox keeps the object out of the RTree.
    p.bbox_size = {};
    u32 i = 0;
    for (i32 z = 0; z < num_z; z++)
        for (i16 y = 0; y < find_side; y++)
            for (i16 x = 0; x < find_side; x++)
            {
                const auto cz = i8(num_z == 1 ? 0 : chunk_z_min + z);
                const chunk_coords_ ch{i16(x - find_side/2), i16(y - find_side/2), cz};
                for (auto k = 0u; k < find_per_chunk; k++)
                    ids[i++] = w.make_object<light>(w.make_id(), {ch, local_coords{k % TILE_COUNT}}, p)->id;
            }

    if (shuffled)
    {
        u64 rng = 0x9e3779b97f4a7c15u;
        for (auto j = num_objects; j-- > 1; )
        {
            rng = rng*6364136223846793005u + 1442695040888963407u;
            const auto k = (u32)((rng >> 32) % (j + 1));
            const auto tmp = ids[j]; ids[j] = ids[k]; ids[k] = tmp;
        }
    }

    for (auto _ : state)
        for (const object_id id : ids)
        {
            auto o = w.find_object(id);
            benchmark::DoNotOptimize(o);
        }
    state.counters["lookup"] = benchmark::Counter(num_objects, benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
}
BENCHMARK(World_FindObject)->ArgNames({"z", "shuffled"})->ArgsProduct({{1, chunk_z_count}, {0, 1}})->Unit(benchmark::kMillisecond);

} // namespace

} // namespace floormat
