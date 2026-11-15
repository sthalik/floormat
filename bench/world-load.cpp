#include "src/world.hpp"
#include "compat/format.inl"
#include "loader/loader.hpp"
#include "loader/policy.hpp"
#include <cr/GrowableArray.h>
#include <cr/Path.h>
#include <cr/String.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

String save_path(benchmark::State& state)
{
    char buf[64];
    const auto len = snformat(buf, "bench/save/proto-{}-quicksave.dat", state.range(0));
    return Path::join(loader.TEMP_PATH, StringView{buf, len});
}

bool setup(benchmark::State& state, StringView path)
{
    if (!Path::exists(path))
    {
        state.SkipWithError("save file not installed");
        return false;
    }
    // the first load also reads every atlas the save refers to
    (void)world::deserialize(path, loader_policy::warn);
    return true;
}

void World_Load(benchmark::State& state)
{
    const auto path = save_path(state);
    if (!setup(state, path))
        return;

    for (auto _ : state)
    {
        auto w = world::deserialize(path, loader_policy::warn);
        benchmark::DoNotOptimize(&w);
    }
}
BENCHMARK(World_Load)->ArgName("proto")->Arg(31)->Arg(32)->Unit(benchmark::kMillisecond);

void World_Load_Keep(benchmark::State& state)
{
    const auto path = save_path(state);
    if (!setup(state, path))
        return;
    Array<world> worlds;
    arrayReserve(worlds, (usize)state.max_iterations);

    for (auto _ : state)
        arrayAppend(worlds, world::deserialize(path, loader_policy::warn));
}
// fixed for the same reason as World_Create_Keep
BENCHMARK(World_Load_Keep)->ArgName("proto")->Arg(31)->Arg(32)->Iterations(32)->Unit(benchmark::kMillisecond);

} // namespace

} // namespace floormat
