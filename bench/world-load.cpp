#include "src/world.hpp"
#include "loader/loader.hpp"
#include "loader/policy.hpp"
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <cr/Path.h>
#include <cr/String.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

String newest_save()
{
    using LF = Path::ListFlag;
    const auto dir = Path::join(loader.TEMP_PATH, "save"_s);
    auto files = Path::list(dir, LF::SkipDirectories|LF::SkipSpecial|LF::SkipDotAndDotDot|LF::SortAscending);
    String ret;
    if (!files)
        return ret;
    int64_t newest = 0;
    for (const StringView file : *files)
    {
        if (!file.hasSuffix(".dat"_s))
            continue;
        auto path = Path::join(dir, file);
        const auto ns = Path::lastModification(path);
        if (ns && (ret.isEmpty() || *ns > newest))
        {
            newest = *ns;
            ret = move(path);
        }
    }
    return ret;
}

bool setup(benchmark::State& state, StringView path)
{
    if (path.isEmpty())
    {
        state.SkipWithError("no .dat file in save/");
        return false;
    }
    const auto name = Path::filename(path);
    state.SetLabel(std::string{name.data(), name.size()});
    // the first load also reads every atlas the save refers to
    (void)world::deserialize(path, loader_policy::warn);
    return true;
}

void World_Load(benchmark::State& state)
{
    const auto path = newest_save();
    if (!setup(state, path))
        return;

    for (auto _ : state)
    {
        auto w = world::deserialize(path, loader_policy::warn);
        benchmark::DoNotOptimize(&w);
    }
}
BENCHMARK(World_Load)->Unit(benchmark::kMillisecond);

void World_Load_Keep(benchmark::State& state)
{
    const auto path = newest_save();
    if (!setup(state, path))
        return;
    Array<world> worlds;
    arrayReserve(worlds, (size_t)state.max_iterations);

    for (auto _ : state)
        arrayAppend(worlds, world::deserialize(path, loader_policy::warn));
}
// fixed for the same reason as World_Create_Keep
BENCHMARK(World_Load_Keep)->Iterations(32)->Unit(benchmark::kMillisecond);

} // namespace

} // namespace floormat
