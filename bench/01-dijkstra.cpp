#include "src/path-search.hpp"
#include "src/path-search-result.hpp"
#include "loader/loader.hpp"
#include <benchmark/benchmark.h>
#include <Corrade/Containers/Optional.h>
#include <Magnum/Math/Functions.h>

namespace floormat {

namespace {

auto A = astar();
bool first_run = true;

void Dijkstra(benchmark::State& state)
{
    auto w = world();

    constexpr auto wcx = 1, wcy = 1, wtx = 8, wty = 8, wox = 3, woy = 3;
    constexpr auto max_dist = (uint32_t)(Vector2i(Math::abs(wcx)+1, Math::abs(wcy)+1)*TILE_MAX_DIM*iTILE_SIZE2).length();
    constexpr auto wch = chunk_coords_{wcx, wcy, 0};
    constexpr auto wt = local_coords{wtx, wty};
    constexpr auto wpos = global_coords{wch, wt};

    auto& ch = w[wch];
    auto metal2 = tile_image_proto{loader.tile_atlas("metal2", {2, 2}, pass_mode::blocked), 0};

    for (int16_t j = wcy - 1; j <= wcy + 1; j++)
        for (int16_t i = wcx - 1; i <= wcx + 1; i++)
        {
            auto &c = w[chunk_coords_{i, j, 0}];
            for (int k : {  3, 4, 5, 6, 11, 12, 13, 14, 15, })
            {
                c[{ k, k }].wall_north() = metal2;
                c[{ k, k }].wall_west() = metal2;
            }
        }

    ch[{ wtx,   wty   }].wall_west()  = metal2;
    ch[{ wtx,   wty   }].wall_north() = metal2;
    ch[{ wtx+1, wty   }].wall_west()  = metal2;
    ch[{ wtx,   wty -1}].wall_north() = metal2;

    for (int16_t j = wcy - 1; j <= wcy + 1; j++)
        for (int16_t i = wcx - 1; i <= wcx + 1; i++)
        {
            auto& c = w[chunk_coords_{i, j, 0}];
            c.mark_passability_modified();
            c.ensure_passability();
        }

    auto run = [&] {
        A.Dijkstra(w,
                 {{0,0,0}, {11,9}},    // from
                 {wpos, {wox, woy}},   // to
                 0, max_dist, {32,32}, // size
                 first_run ? 1 : 0);
    };

    run();
    first_run = false;
    for (auto _ : state)
        run();
}

} // namespace

BENCHMARK(Dijkstra);

} // namespace floormat