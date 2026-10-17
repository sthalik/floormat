#include "src/grid-pass.hpp"
#include "src/grid-cover.hpp"
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

    Pass::Pool pool{Pass::Params{(uint32_t)state.range(0), pass_mode::pass}};
    pool.maybe_mark_stale_all(w);
    pool.build_if_stale_all(Search::without_critters());

    Pass::Grid g1 = pool[c1];
    Pass::Grid g2 = pool[c2];
    Pass::Grid g3 = pool[c3];
    Pass::Grid g4 = pool[c4];
    Pass::Grid g5 = pool[c5];
    Pass::Grid g6 = pool[c6];
    Pass::Grid g7 = pool[c7];
    g1.build_if_stale(Search::without_critters());
    g2.build_if_stale(Search::without_critters());
    g3.build_if_stale(Search::without_critters());
    g4.build_if_stale(Search::without_critters());
    g5.build_if_stale(Search::without_critters());
    g6.build_if_stale(Search::without_critters());
    g7.build_if_stale(Search::without_critters());

    for (auto _ : state)
    {
        g1.mark_stale();
        g2.mark_stale();
        g3.mark_stale();
        g4.mark_stale();
        g5.mark_stale();
        g6.mark_stale();
        g7.mark_stale();

        g1.build_if_stale(Search::without_critters());
        g2.build_if_stale(Search::without_critters());
        g3.build_if_stale(Search::without_critters());
        g4.build_if_stale(Search::without_critters());
        g5.build_if_stale(Search::without_critters());
        g6.build_if_stale(Search::without_critters());
        g7.build_if_stale(Search::without_critters());
    }
}

BENCHMARK(Grid_Build)->Arg(16)->Arg(8)->Unit(benchmark::kMillisecond);

void make_cover_chunk(world& w, chunk& c)
{
    const auto floor = tile_image_proto{ loader.ground_atlas("texel"), 0 };
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

    Cover::Pool pool{Cover::Params{(uint32_t)state.range(0)}};
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

    Cover::Pool pool{Cover::Params{(uint32_t)state.range(0)}};
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

} // namespace

} // namespace floormat
