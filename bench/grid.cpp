#include "src/grid-pass.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-pool-registry.hpp"
#include "src/cover-point.hpp"
#include "src/point.inl"
#include "src/intra-coord.inl"
#include "src/search.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/scenery-proto.hpp"
#include "src/tile-image.hpp"
#include "src/wall-atlas.hpp"
#include "src/tile-defs.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include <bit>
#include <cr/Optional.h>
#include <mg/Functions.h>
#include <mg/Range.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

chunk& make_chunk1(chunk& c, bool val, bool flipped)
{
    auto floor = tile_image_proto { loader.ground_atlas("texel"), 0 };
    auto empty = tile_image_proto{};

    constexpr uint8_t mat[TILE_MAX_DIM][TILE_MAX_DIM] = {
        { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        { 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, },
        { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    };

    if (!flipped)
        for (auto j = 0u; j < TILE_MAX_DIM; j++)
            for (auto i = 0u; i < TILE_MAX_DIM; i++)
                c[{i, j}].ground() = !!mat[i][j] == val ? floor : empty;
    else
        for (auto j = 0u; j < TILE_MAX_DIM; j++)
            for (auto i = 0u; i < TILE_MAX_DIM; i++)
                c[{i, j}].ground() = !!mat[j][i] == val ? floor : empty;

    return c;
}

chunk& make_chunk3(chunk& c, bool do_empty)
{
    auto floor = tile_image_proto { loader.ground_atlas("texel"), 0 };
    auto empty = tile_image_proto{};

    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = floor;
    if (do_empty)
        c[{0, 0}].ground() = empty;
    return c;
}

void rebuild(chunk& c)
{
    c.mark_passability_modified();
    c.ensure_passability();
}

void Grid_Build(benchmark::State& state)
{
    auto w = world();
    auto& c1 = w[{1, 0, 0}];
    auto& c2 = make_chunk1(w[{2, 0, 0}], true, false);
    auto& c3 = make_chunk3(w[{3, 0, 0}], false);
    auto& c4 = make_chunk3(w[{4, 0, 0}], true);
    auto& c5 = make_chunk1(w[{5, 0, 0}], false, true);
    auto& c6 = make_chunk1(w[{6, 0, 0}], false, false);
    auto& c7 = make_chunk1(w[{7, 0, 0}], true, true);

    for (auto* cʹ : { &c1, &c2, &c3, &c4, &c5, &c6, &c7 })
        rebuild(*cʹ);

    Pass::Pool pool{Pass::Params{(uint32_t)state.range(0), pass_mode::pass}, Search::without_critters()};
    pool.maybe_mark_stale_all(w);
    pool.build_if_stale_all();

    Pass::Grid g1 = pool[c1];
    Pass::Grid g2 = pool[c2];
    Pass::Grid g3 = pool[c3];
    Pass::Grid g4 = pool[c4];
    Pass::Grid g5 = pool[c5];
    Pass::Grid g6 = pool[c6];
    Pass::Grid g7 = pool[c7];
    g1.build_if_stale();
    g2.build_if_stale();
    g3.build_if_stale();
    g4.build_if_stale();
    g5.build_if_stale();
    g6.build_if_stale();
    g7.build_if_stale();

    for (auto _ : state)
    {
        g1.mark_stale();
        g2.mark_stale();
        g3.mark_stale();
        g4.mark_stale();
        g5.mark_stale();
        g6.mark_stale();
        g7.mark_stale();

        g1.build_if_stale();
        g2.build_if_stale();
        g3.build_if_stale();
        g4.build_if_stale();
        g5.build_if_stale();
        g6.build_if_stale();
        g7.build_if_stale();
    }
}

BENCHMARK(Grid_Build)->Arg(16)->Arg(8)->Unit(benchmark::kMillisecond);

void make_cover_chunk(world& w, chunk& c)
{
    const auto floor = tile_image_proto{ loader.ground_atlas("tiles"), 0 };
    const auto wall = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    const auto ch = c.coord();

    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
        {
            c[{i, j}].ground() = floor;
            if (j == 5 && i != 7 && i != 8)
                c[{i, j}].wall_north() = wall;
            if (i == 10 && j != 3 && j != 11)
                c[{i, j}].wall_west() = wall;
            if ((i*7 + j*3) % 4 == 0)
            {
                scenery_proto p;
                p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
                p.subtype   = generic_scenery_proto{};
                p.bbox_size = {24, 24};
                p.pass      = pass_mode::blocked;
                w.make_scenery(w.make_id(), {ch, {i, j}}, move(p));
            }
            else if ((i*5 + j) % 23 == 0)
                (void)w.make_object<critter>(w.make_id(), {ch, {i, j}}, critter_proto{});
        }
}

void Cover_Build(benchmark::State& state)
{
    auto w = world();
    auto& c = w[{0, 0, 0}];
    make_cover_chunk(w, c);
    rebuild(c);

    const auto div_size = (uint32_t)state.range(0);
    Pass::Pool pass_pool{Pass::Params{div_size, pass_mode::shoot_through}, Search::without_critters()};
    Cover::Pool pool{Cover::Params{div_size}, pass_pool};
    pool.maybe_mark_stale_all(w);
    Cover::Grid g = pool[c];

    for (auto _ : state)
    {
        g.mark_stale();
        g.build_if_stale();
        // octant 0 is axis-aligned and mostly copies neighbour cells, octant 1 casts a ray from every cell
        (void)g.ensure_octant(1);
    }
}

BENCHMARK(Cover_Build)->Arg(16)->Arg(8)->Unit(benchmark::kMillisecond);

// The eight octants that sweep the pass bitmap. Only cells on the far edge cast a ray.
void Cover_Aligned(benchmark::State& state)
{
    auto w = world();
    auto& c = w[{0, 0, 0}];
    make_cover_chunk(w, c);
    rebuild(c);

    const auto div_size = (uint32_t)state.range(0);
    Pass::Pool pass_pool{Pass::Params{div_size, pass_mode::shoot_through}, Search::without_critters()};
    Cover::Pool pool{Cover::Params{div_size}, pass_pool};
    pool.maybe_mark_stale_all(w);
    Cover::Grid g = pool[c];

    for (auto _ : state)
    {
        g.mark_stale();
        g.build_if_stale();
        for (auto k = 4u; k < Cover::octant_count; k += 4)
            (void)g.ensure_octant(k);
    }
}

BENCHMARK(Cover_Aligned)->Arg(64)->Arg(16)->Arg(8)->Arg(2)->Unit(benchmark::kMicrosecond);

constexpr auto cover_body = Vector2ui{24, 24};

template<typename F> void for_cover_chunks(F&& fun)
{
    for (int16_t y = -1; y <= 1; y++)
        for (int16_t x = -1; x <= 1; x++)
            fun(chunk_coords_{x, y, 0});
}

void make_cover_world(world& w)
{
    for_cover_chunks([&](chunk_coords_ ch) { make_cover_chunk(w, w[ch]); });
    for_cover_chunks([&](chunk_coords_ ch) { rebuild(w[ch]); });
}

struct find_case
{
    Vector2i npc;
    float bearing_deg, dist;
};

// The npc stands on a tile corner in chunk (0, 0). Case 4's threat is past the cover grid's 1024 px cap.
constexpr find_case find_cases[] = {
    { {  96, 544 },  10,  256 },
    { { 800,  96 }, 100,  192 },
    { { 416, 800 }, 200,  512 },
    { { 864, 864 }, 300,  384 },
    { { 160, 160 }, 140, 1536 },
    { { 352, 416 }, 250,   96 },
};

cover_query make_cover_query(world& w, const find_case& f)
{
    const auto a = Rad{Deg{f.bearing_deg}};
    const auto threat = f.npc + Vector2i(Math::round(Vector2{Math::cos(a), Math::sin(a)} * f.dist));
    const auto npc = point{Vector3i{f.npc, 0}};
    const auto min = Vector2(npc.intra().center_shifted() - Vector2i(cover_body/2u));
    fm_assert(Search::is_passable(w, npc.chunk3(), Range2D{min, min + Vector2(cover_body)}, Search::never_continue()));
    return { npc, cover_body, point{Vector3i{threat, 0}}, 3*(uint32_t)tile_size_xy };
}

void set_find_counters(benchmark::State& state, const cover_point_result& r)
{
    state.counters["found"] = r.found;
    state.counters["candidates"] = r.candidates;
}

// Octants built on the 3×3 chunks, the ones casting a ray per cell apart from those sweeping the pass bitmap.
void set_fill_counters(benchmark::State& state, world& w, Cover::Pool& pool, uint32_t sweeps_before)
{
    constexpr uint32_t sweep_mask = 0x11111111u;
    uint32_t rays = 0, sweeps = 0;
    for_cover_chunks([&](chunk_coords_ ch) {
        const auto k = pool[w[ch]].built_octants();
        rays += (uint32_t)std::popcount(k & ~sweep_mask);
        sweeps += (uint32_t)std::popcount(k & sweep_mask);
    });
    state.counters["ray_fills"] = rays;
    state.counters["sweep_fills"] = sweeps - sweeps_before;
}

// Every grid and octant the query reads is built, as for each later seek from the same threat.
void Cover_Find(benchmark::State& state)
{
    auto w = world();
    make_cover_world(w);
    const auto params = Cover::Params{(uint32_t)state.range(0)};
    const auto q = make_cover_query(w, find_cases[state.range(1)]);
    Grid::PoolRegistry pools{params.div_size};
    auto r = find_cover_point(w, pools, params, q);
    for (auto _ : state)
        r = find_cover_point(w, pools, params, q);
    set_find_counters(state, r);
}

// The first seek from a threat at a bearing no cover grid has seen: the pass grids and cover grids
// are built, but only octant 0 of each.
void Cover_Find_Octants(benchmark::State& state)
{
    auto w = world();
    make_cover_world(w);
    const auto params = Cover::Params{(uint32_t)state.range(0)};
    const auto q = make_cover_query(w, find_cases[state.range(1)]);
    Optional<Grid::PoolRegistry> pools;
    cover_point_result r;
    for (auto _ : state)
    {
        state.PauseTiming();
        pools = NullOpt;
        pools.emplace(params.div_size);
        auto& pass_pool = pools->pass_pool(Pass::Params{params.div_size, pass_mode::shoot_through});
        auto& cover_pool = pools->cover_pool(params);
        for_cover_chunks([&](chunk_coords_ ch) {
            pass_pool[w[ch]].build_if_stale();
            cover_pool[w[ch]].build_if_stale();
        });
        state.ResumeTiming();
        r = find_cover_point(w, *pools, params, q);
    }
    set_find_counters(state, r);
    set_fill_counters(state, w, pools->cover_pool(params), 9);
}

// The first seek in an area: every grid the query reads is built from nothing.
void Cover_Find_Fresh(benchmark::State& state)
{
    auto w = world();
    make_cover_world(w);
    const auto params = Cover::Params{(uint32_t)state.range(0)};
    const auto q = make_cover_query(w, find_cases[state.range(1)]);
    Optional<Grid::PoolRegistry> pools;
    cover_point_result r;
    for (auto _ : state)
    {
        state.PauseTiming();
        pools = NullOpt;
        pools.emplace(params.div_size);
        state.ResumeTiming();
        r = find_cover_point(w, *pools, params, q);
    }
    set_find_counters(state, r);
    set_fill_counters(state, w, pools->cover_pool(params), 0);
}

void find_args(benchmark::Benchmark* b)
{
    b->ArgNames({"div", "case"})->ArgsProduct({{8, 16}, benchmark::CreateDenseRange(0, (int64_t)std::size(find_cases) - 1, 1)});
}

BENCHMARK(Cover_Find)->Apply(find_args)->Unit(benchmark::kMicrosecond);
BENCHMARK(Cover_Find_Octants)->Apply(find_args)->Unit(benchmark::kMillisecond);
BENCHMARK(Cover_Find_Fresh)->Apply(find_args)->Unit(benchmark::kMillisecond);

} // namespace

} // namespace floormat
