#include "cover-world.hpp"
#include "src/search-result.hpp"
#include "src/search-astar.hpp"
#include "src/grid-pass.hpp"
#include "src/critter.hpp"
#include "src/critter-script-cover-spot-tests.hpp"
#include "src/tile-image.hpp"
#include "src/point.hpp"
#include "src/world.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function-view.hpp"
#include "loader/loader.hpp"
#include <benchmark/benchmark.h>
#include <cr/Optional.h>
#include <mg/Functions.h>

namespace floormat {

namespace {

void Dijkstra(benchmark::State& state)
{
    (void)loader.wall_atlas_list();
    auto w = world();
    auto A = astar();

    constexpr auto wcx = 1, wcy = 1, wtx = 8, wty = 8, wox = 0, woy = 0;
    constexpr auto max_dist = (u32)(Vector2i(Math::abs(wcx)+1, Math::abs(wcy)+1)*chunk_size<Vector2i>).length();
    constexpr auto wch = chunk_coords_{wcx, wcy, 0};
    constexpr auto wt = local_coords{wtx, wty};
    constexpr auto wpos = global_coords{wch, wt};
    const auto from = point{{0, 0, 0}, {11, 9}};
    constexpr auto own_size = Vector2ui{16};
    const auto cell = Pass::Params{astar::default_div_size, own_size.x(), pass_mode::pass}.div_size();

    // Same floors as add_floors() in test/dijkstra.cpp.
    const auto floor = tile_image_proto{ loader.ground_atlas("tiles"), 0 };
    const auto reach = Vector2i{(int)(max_dist + cell)};
    const auto lo = (from - reach).chunk(), hi = (from + reach).chunk();
    for (auto j = lo.y; j <= hi.y; j++)
        for (auto i = lo.x; i <= hi.x; i++)
        {
            auto& c = w[{i, j, 0}];
            for (auto y = 0u; y < TILE_MAX_DIM; y++)
                for (auto x = 0u; x < TILE_MAX_DIM; x++)
                    c[{x, y}].ground() = floor;
            c.mark_passability_modified();
            c.ensure_passability();
        }

    auto& ch = w[wch];
    auto metal2 = wall_image_proto{loader.wall_atlas("empty", loader_policy::warn), 0};

    for (i16 j = wcy - 1; j <= wcy + 1; j++)
        for (i16 i = wcx - 1; i <= wcx + 1; i++)
        {
            auto &c = w[{i, j, 0}];
            for (int k : {  3, 4, 5, 6, 11, 12, 13, 14, 15, })
            {
                c[{ k, k }].wall_north() = metal2;
                c[{ k, k }].wall_west() = metal2;
            }
        }

    ch[{ wtx,   wty   }].wall_west()  = metal2;
    ch[{ wtx,   wty   }].wall_north() = metal2;
    ch[{ wtx+1, wty   }].wall_west()  = metal2;
    ch[{ wtx,   wty +1}].wall_north() = metal2;

    for (i16 j = wcy - 1; j <= wcy + 1; j++)
        for (i16 i = wcx - 1; i <= wcx + 1; i++)
        {
            auto& c = w[{i, j, 0}];
            c.mark_passability_modified();
            c.ensure_passability();
        }

    auto run = [&]<int Debug> {
        return A.Dijkstra<Debug>(w, from,
                                 {wpos, {wox, woy}},   // to
                                 max_dist, own_size,
                                 Search::without_critters);
    };

    {
        auto res = run.operator()<0>();
        fm_assert(!res.is_found());
        fm_assert(res.distance() < 128);
        fm_assert(res.distance() > 8);
        fm_assert(res.cost() > 1800);
        fm_assert(res.cost() < 3000);
    }

    for (int i = 0; i < 3; i++)
        run.operator()<0>();
    for (auto _ : state)
        run.operator()<0>();
}

template<typename F>
void time_spot_tests_call(benchmark::State& st, F&& find_path)
{
    const auto dist = (int)st.range(0);
    const bool walled_in = st.range(1), scenery = st.range(2);
    constexpr auto body = Vector2ui{24, 24};
    constexpr auto ch = chunk_coords_{0, 0, 0};
    constexpr auto npc_tile = local_coords{2, 8};
    const auto to_tile = local_coords{(u8)(npc_tile.x + dist/tile_size_xy), npc_tile.y};

    auto w = world();
    if (scenery)
        make_cover_world(w);
    else
    {
        const auto floor = tile_image_proto{loader.ground_atlas("tiles"), 0};
        for_cover_chunks([&](chunk_coords_ c) {
            for (auto j = 0u; j < TILE_MAX_DIM; j++)
                for (auto i = 0u; i < TILE_MAX_DIM; i++)
                    w[c][{i, j}].ground() = floor;
        });
    }
    if (walled_in)
    {
        const auto wall = wall_image_proto{loader.wall_atlas("empty"), 0};
        auto& c = w[ch];
        c[to_tile].wall_north() = wall;
        c[to_tile].wall_west() = wall;
        c[{(u8)(to_tile.x + 1), to_tile.y}].wall_west() = wall;
        c[{to_tile.x, (u8)(to_tile.y + 1)}].wall_north() = wall;
    }
    for_cover_chunks([&](chunk_coords_ c) {
        w[c].mark_passability_modified();
        w[c].ensure_passability();
    });

    critter_proto p;
    p.bbox_size = Vector2ub(body);
    const auto npc = w.make_object<critter>(w.make_id(), global_coords{ch, npc_tile}, move(p));
    auto A = astar{};
    auto t = Cover::spot_tests{A, 0, NullOpt};
    const auto to = point{ch, to_tile, {}};

    fm_assert(find_path(t, *npc, to) == !walled_in);
    const auto added_node_count = t.last_dijkstra_node_count();
    fm_assert(added_node_count > 0);
    for (int i = 0; i < 2; i++)
        (void)find_path(t, *npc, to);
    for (auto _ : st)
        benchmark::DoNotOptimize(find_path(t, *npc, to));
    st.counters["added_node_count"] = added_node_count;
}

void Dijkstra_Path_To(benchmark::State& st)
{
    time_spot_tests_call(st, [](Cover::spot_tests& t, critter& c, point to) {
        return t.path_to(c, to).is_found();
    });
}

void Dijkstra_Reachable_Without_Critters(benchmark::State& st)
{
    time_spot_tests_call(st, [](Cover::spot_tests& t, critter& c, point to) {
        return t.is_reachable_without_critters(c, to);
    });
}

void spot_tests_args(benchmark::Benchmark* b)
{
    b->ArgNames({"dist", "walled_in", "scenery"})
        ->ArgsProduct({{3*tile_size_xy, 4*tile_size_xy, 5*tile_size_xy}, {0, 1}, {0, 1}})
        ->Unit(benchmark::kMicrosecond);
}

} // namespace

BENCHMARK(Dijkstra)->Unit(benchmark::kMillisecond);
BENCHMARK(Dijkstra_Path_To)->Apply(spot_tests_args);
BENCHMARK(Dijkstra_Reachable_Without_Critters)->Apply(spot_tests_args);

} // namespace floormat
