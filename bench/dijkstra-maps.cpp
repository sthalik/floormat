#include "editor/mazes.hpp"
#include "src/search-result.hpp"
#include "src/search-astar.hpp"
#include "src/search-cache.hpp"
#include "src/search-pred.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-pass-pool.hpp"
#include "src/point.inl"
#include "src/world.hpp"
#include "compat/assert.hpp"
#include "compat/function2.hpp"
#include <benchmark/benchmark.h>
#include <mg/Functions.h>

namespace floormat {

namespace {

using mazes::kind;

struct map_world
{
    world w;
    astar A;
    mazes::route r;

    explicit map_world(kind k):
        r{mazes::build(w, k, chunk_coords_{0, 0, 0}, A.pass_pool_registry().pool_for(mazes::bbox_size))}
    {}
};

path_search_result search(map_world& s, const Search::heuristic& h = Search::octile_distance())
{
    return s.A.Dijkstra<0, false>(s.w, s.r.from, s.r.to, s.r.max_dist,
                                  Vector2ui{mazes::bbox_size},
                                  Search::without_critters(), h);
}

void verify(kind k)
{
    map_world s{k};
    const auto& h = Search::octile_distance();

    const auto res = search(s);
    // Seeds get g = h(from, seed), so they keep the octile cost.
    const auto zero = [&](point cur, point goal) -> uint32_t { return goal == s.r.to ? 0 : h(cur, goal); };
    const auto opt = search(s, Search::heuristic{zero});
    fm_assert(res.is_found());
    fm_assert(opt.is_found());
    // octile h can exceed the goal hop's cost by 1, so A* may end 1 above the optimum
    fm_assert(res.cost() >= opt.cost() && res.cost() <= opt.cost() + 1);

    auto& pool = s.A.pass_pool_registry().pool_for(mazes::bbox_size);
    constexpr auto nch = mazes::map_chunks - 1;
    const auto div_count = pool.params().div_count, side = div_count*(uint32_t)nch;
    uint32_t open_cells = 0;
    for (int16_t cy = 0; cy < nch; cy++)
        for (int16_t cx = 0; cx < nch; cx++)
        {
            auto g = pool[s.w[chunk_coords_{cx, cy, 0}]];
            g.build_if_stale(Search::without_critters());
            for (auto j = 0u; j < div_count; j++)
                for (auto i = 0u; i < div_count; i++)
                    open_cells += g.bit(Pass::Grid::get_bitmask_index(i, j, div_count));
        }

    const auto rect = Search::cache::get_size_to_allocate(s.r.max_dist)*2u + Vector2ui{1};
    const auto h0 = h(s.r.from, s.r.to);
    DBG_nospace << "dijkstra-maps " << mazes::name(k)
                << ": open cells " << open_cells << "/" << side*side
                << ", bfs tiles " << s.r.bfs_tiles
                << ", cost " << res.cost() << ", optimum " << opt.cost()
                << ", h " << h0 << ", cost/h " << (float)opt.cost()/(float)h0
                << ", max_dist " << s.r.max_dist
                << ", cache chunks " << rect.product();
}

// A world per call, as in Dijkstra<>. A static one is destroyed after bench_app's loader.destroy(),
// and a leaked one trips the RTree pool destructor's assert that every tree came back.
template<kind K>
void bench_map(benchmark::State& state)
{
    static const bool verified = (verify(K), true);
    (void)verified;
    map_world s{K};
    for (int i = 0; i < 2; i++)
        search(s);
    for (auto _ : state)
        search(s);
}

void Dijkstra_Rooms(benchmark::State& state) { bench_map<kind::rooms>(state); }
void Dijkstra_Braid(benchmark::State& state) { bench_map<kind::braid>(state); }
void Dijkstra_Prim(benchmark::State& state) { bench_map<kind::prim>(state); }

} // namespace

BENCHMARK(Dijkstra_Rooms)->Unit(benchmark::kMillisecond);
BENCHMARK(Dijkstra_Braid)->Unit(benchmark::kMillisecond);
BENCHMARK(Dijkstra_Prim)->Unit(benchmark::kMillisecond);

} // namespace floormat
