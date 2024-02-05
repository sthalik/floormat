#include "loader/loader.hpp"
#include "loader/ground-info.hpp"
#include "loader/wall-info.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StringIterable.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

constexpr struct {
    const char* name;
    Vector2ub size;
    pass_mode pass = pass_mode::pass;
} ground_atlases[] = {
    { "floor-tiles", {44,4} },
    { "tiles", {8, 5} },
    { "texel", {2, 2}, pass_mode::blocked },
    { "metal1", {2,2} },
};

constexpr const char* wall_atlases[] = {
    "concrete1", "empty", "test1",
};

constexpr const char* anim_atlases[] = {
    "anim/npc-walk",
    "anim/test-8x8",
    "scenery/door-close",
    "scenery/control-panel",
    "scenery/table",
};

using nlohmann::json;

void run()
{
    for (const auto& str : anim_atlases)
        (void)loader.get_anim_atlas(str);
    for (const auto& x : ground_atlases)
        (void)loader.get_ground_atlas(x.name, x.size, x.pass);
    for (const auto& name : wall_atlases)
        (void)loader.get_wall_atlas(name);
}

void Loader_json(benchmark::State& state)
{
    loader.destroy();

    run();
    for (auto _ : state)
        run();
}

BENCHMARK(Loader_json)->Unit(benchmark::kMillisecond);

} // namespace

} // namespace floormat
