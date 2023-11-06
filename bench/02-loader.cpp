#include "loader/loader.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Utility/Path.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

void Loader_json(benchmark::State& state)
{
    loader.destroy();

    // warmup
    {   for (const auto& x : loader.anim_atlas_list())
            json_helper::from_json<anim_def>(Path::join(loader.ANIM_PATH, ""_s.join({x, ".json"})));
        json_helper::from_json<std::vector<nlohmann::json>>(Path::join(loader.VOBJ_PATH, "vobj.json"));
    }

    for (auto _ : state)
        for (int i = 0; i < 10; i++)
        {
            for (const auto& x : loader.anim_atlas_list())
                json_helper::from_json<anim_def>(Path::join(loader.ANIM_PATH, ""_s.join({x, ".json"})));
            json_helper::from_json<std::vector<nlohmann::json>>(Path::join(loader.VOBJ_PATH, "vobj.json"));
        }
}

BENCHMARK(Loader_json)->Unit(benchmark::kMillisecond)->ReportAggregatesOnly();

} // namespace

} // namespace floormat
