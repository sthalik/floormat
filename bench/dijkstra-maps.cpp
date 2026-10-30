#include "editor/mazes.hpp"
#include "src/search-result.hpp"
#include "src/search-astar.hpp"
#include "src/search-cache.hpp"
#include "src/search-pred.hpp"
#include "src/search-octile.hpp"
#include "src/grid-pass.hpp"
#include "src/raycast-line.hpp"
#include "src/sweep-aabb.hpp"
#include "src/point.inl"
#include "src/world.hpp"
#include "src/tile-image.hpp"
#include "loader/loader.hpp"
#include "compat/assert.hpp"
#include "compat/function2.hpp"
#include <benchmark/benchmark.h>
#include <mg/Functions.h>

namespace floormat {

namespace {

using mazes::kind;

constexpr uint32_t default_cell = 16;

struct map_world
{
    world w;
    astar A;
    Pass::Pool pool;
    mazes::route r;

    map_world(kind k, uint32_t cell):
        pool{Pass::Params{cell, mazes::bbox_size, pass_mode::pass}, Search::without_critters()},
        r{mazes::build(w, k, chunk_coords_{0, 0, 0}, pool)}
    {}
};

path_search_result search(map_world& s)
{
    return s.A.Dijkstra<0, false>(s.w, s.pool, s.r.from, s.r.to, s.r.max_dist,
                                  Vector2ui{mazes::bbox_size},
                                  Search::without_critters());
}

void verify(kind k, uint32_t cell, uint32_t optimum)
{
    map_world s{k, cell};

    const auto res = search(s);
    fm_assert(res.is_found());
    if (res.cost() != optimum)
        fm_abort("dijkstra-maps %s cell %u: cost %u, optimum %u",
                 mazes::name(k).data(), cell, res.cost(), optimum);

    auto& pool = s.pool;
    constexpr auto nch = mazes::map_chunks - 1;
    const auto div_count = pool.params().div_count, side = div_count*(uint32_t)nch;
    uint32_t open_cells = 0;
    for (int16_t cy = 0; cy < nch; cy++)
        for (int16_t cx = 0; cx < nch; cx++)
        {
            auto g = pool[s.w[chunk_coords_{cx, cy, 0}]];
            g.build_if_stale();
            for (auto j = 0u; j < div_count; j++)
                for (auto i = 0u; i < div_count; i++)
                    open_cells += g.bit(Pass::Grid::get_bitmask_index(i, j, div_count));
        }

    const auto rect = Search::cache::get_size_to_allocate(s.r.max_dist)*2u + Vector2ui{1};
    const auto h0 = Search::octile{pool.params().div_size()}(s.r.from, s.r.to);
    DBG_nospace << "dijkstra-maps " << mazes::name(k) << " cell " << pool.params().div_size()
                << ": open cells " << open_cells << "/" << side*side
                << ", bfs tiles " << s.r.bfs_tiles
                << ", cost " << res.cost() << ", optimum " << optimum
                << ", h " << h0 << ", cost/h " << (float)optimum/(float)h0
                << ", max_dist " << s.r.max_dist
                << ", cache chunks " << rect.product();
}

// A world per call, as in Dijkstra<>. A static one is destroyed after bench_app's loader.destroy(),
// and a leaked one trips the RTree pool destructor's assert that every tree came back.
template<kind K, uint32_t Cell = default_cell>
void bench_map(benchmark::State& state, uint32_t optimum)
{
    static const bool verified = (verify(K, Cell, optimum), true);
    (void)verified;
    map_world s{K, Cell};
    for (int i = 0; i < 2; i++)
        search(s);
    for (auto _ : state)
        search(s);
}

template<kind K>
path_search_result pulled_path(map_world& s)
{
    auto res = search(s);
    fm_assert(res.is_found());
    fm_assert(s.A.string_pull(s.w, s.pool, res, Vector2ui{mazes::bbox_size}, Search::without_critters()));
    return res;
}

template<kind K>
void bench_pull(benchmark::State& state)
{
    map_world s{K, default_cell};
    auto res = pulled_path<K>(s);
    for (auto _ : state)
        benchmark::DoNotOptimize(s.A.string_pull(s.w, s.pool, res, Vector2ui{mazes::bbox_size}, Search::without_critters()));
}

// Without a pool, as move_toward sweeps each frame's steps.
template<kind K>
void bench_sweep(benchmark::State& state)
{
    map_world s{K, default_cell};
    const auto res = pulled_path<K>(s);
    const auto wp = res.smoothings();
    const auto sweep = [&](size_t i) {
        const auto ch = wp[i].pt.chunk3();
        const auto origin = point{ch, {}, {}};
        const auto line = rc::bresenham_line{wp[i].pt - origin, wp[i+1].pt - origin};
        return find_swept_collider(s.w, ch, line, 0, line.steps(), Vector2ui{mazes::bbox_size}, Search::without_critters());
    };
    for (auto i = 0uz; i + 1 < wp.size(); i++)
        fm_assert(!sweep(i).has_collider);
    for (auto _ : state)
        for (auto i = 0uz; i + 1 < wp.size(); i++)
            benchmark::DoNotOptimize(sweep(i));
}

// Prim's maze with its west seal opened at the NW cell, and `from` on a floored chunk three chunks
// west of the opening. The two chunks between them don't exist. The route through the maze is many
// times the straight line, so A* expands every cell of both. Walled chunks north and south keep it
// from leaving through the missing chunks.
struct missing_chunks_world
{
    world w;
    astar A;
    Pass::Pool pool;
    point from, to;
    uint32_t max_dist;

    missing_chunks_world():
        pool{Pass::Params{default_cell, mazes::bbox_size, pass_mode::pass}, Search::without_critters()}
    {
        const auto r = mazes::build(w, kind::prim, chunk_coords_{0, 0, 0}, pool);
        const auto ground = tile_image_proto{loader.ground_atlas("floor-tiles"), 0};
        const auto wall = wall_image_proto{loader.wall_atlas("empty", loader_policy::warn), 0};

        // The NW cell is tiles (0, 0) to (1, 1). build()'s `from` is the tile farthest from it.
        auto& c0 = w[chunk_coords_{0, 0, 0}];
        c0[local_coords{0, 0}].wall_west() = {};
        c0[local_coords{0, 1}].wall_west() = {};
        c0.mark_modified();
        c0.ensure_passability();

        // A wall on every tile edge, except inside the start chunk. The missing chunks are x -2
        // and -1 at y 0, and the maze holds x 0 from y 0 on.
        for (int16_t cy = -1; cy <= 1; cy++)
            for (int16_t cx = -3; cx <= 0; cx++)
            {
                if ((cy == 0 && cx >= -2) || (cy == 1 && cx == 0))
                    continue;
                const bool start = cx == -3 && cy == 0;
                auto& c = w[chunk_coords_{cx, cy, 0}];
                for (uint8_t ly = 0; ly < TILE_MAX_DIM; ly++)
                    for (uint8_t lx = 0; lx < TILE_MAX_DIM; lx++)
                    {
                        auto t = c[local_coords{lx, ly}];
                        t.ground() = ground;
                        if (!start || lx == 0)
                            t.wall_west() = wall;
                        if (!start || ly == 0)
                            t.wall_north() = wall;
                    }
                c.mark_modified();
                c.ensure_passability();
            }

        from = point{chunk_coords_{-3, 0, 0}, local_coords{8, 8}, {}};
        to = r.from;
        // build()'s max_dist bounds the route from the opening, and the walk to it is under 3 chunks.
        max_dist = r.max_dist + 3*chunk_size_xy;
    }

    path_search_result search()
    {
        return A.Dijkstra<0, false>(w, pool, from, to, max_dist, Vector2ui{mazes::bbox_size},
                                    Search::without_critters());
    }
};

void bench_missing_chunks(benchmark::State& state, uint32_t optimum)
{
    missing_chunks_world s;
    {
        const auto res = s.search();
        fm_assert(res.is_found());
        if (res.cost() != optimum)
            fm_abort("dijkstra-maps missing chunks: cost %u, optimum %u", res.cost(), optimum);
    }
    for (int i = 0; i < 2; i++)
        s.search();
    for (auto _ : state)
        s.search();
}

// Optimal costs. A step is one cell, its costs as in Search::octile.
void Dijkstra_Missing_Chunks(benchmark::State& state) { bench_missing_chunks(state, 17115); }
void Dijkstra_Rooms(benchmark::State& state) { bench_map<kind::rooms>(state, 14397); }
void Dijkstra_Braid(benchmark::State& state) { bench_map<kind::braid>(state, 14451); }
void Dijkstra_Prim(benchmark::State& state) { bench_map<kind::prim>(state, 24637); }
void Dijkstra_Braid_Cell4(benchmark::State& state) { bench_map<kind::braid, 4>(state, 14474); }
void Dijkstra_Braid_Cell8(benchmark::State& state) { bench_map<kind::braid, 8>(state, 14548); }
void Dijkstra_Braid_Cell32(benchmark::State& state) { bench_map<kind::braid, 32>(state, 16288); }
void String_Pull_Rooms(benchmark::State& state) { bench_pull<kind::rooms>(state); }
void String_Pull_Braid(benchmark::State& state) { bench_pull<kind::braid>(state); }
void String_Pull_Prim(benchmark::State& state) { bench_pull<kind::prim>(state); }
void Swept_Collider_Rooms(benchmark::State& state) { bench_sweep<kind::rooms>(state); }
void Swept_Collider_Braid(benchmark::State& state) { bench_sweep<kind::braid>(state); }
void Swept_Collider_Prim(benchmark::State& state) { bench_sweep<kind::prim>(state); }

} // namespace

BENCHMARK(Dijkstra_Missing_Chunks)->Unit(benchmark::kMillisecond);
BENCHMARK(Dijkstra_Rooms)->Unit(benchmark::kMillisecond);
BENCHMARK(Dijkstra_Braid)->Unit(benchmark::kMillisecond);
BENCHMARK(Dijkstra_Prim)->Unit(benchmark::kMillisecond);
BENCHMARK(Dijkstra_Braid_Cell4)->Unit(benchmark::kMillisecond);
BENCHMARK(Dijkstra_Braid_Cell8)->Unit(benchmark::kMillisecond);
BENCHMARK(Dijkstra_Braid_Cell32)->Unit(benchmark::kMillisecond);
BENCHMARK(String_Pull_Rooms)->Unit(benchmark::kMicrosecond);
BENCHMARK(String_Pull_Braid)->Unit(benchmark::kMicrosecond);
BENCHMARK(String_Pull_Prim)->Unit(benchmark::kMicrosecond);
BENCHMARK(Swept_Collider_Rooms)->Unit(benchmark::kMicrosecond);
BENCHMARK(Swept_Collider_Braid)->Unit(benchmark::kMicrosecond);
BENCHMARK(Swept_Collider_Prim)->Unit(benchmark::kMicrosecond);

} // namespace floormat
