#include "src/grid-pass.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/tile-defs.hpp"
#include "loader/loader.hpp"
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
    auto& c1 = w[chunk_coords_{1, 0, 0}];
    auto& c2 = make_chunk1(w[chunk_coords_{2, 0, 0}], true, false);
    auto& c3 = make_chunk3(w[chunk_coords_{3, 0, 0}], false);
    auto& c4 = make_chunk3(w[chunk_coords_{4, 0, 0}], true);
    auto& c5 = make_chunk1(w[chunk_coords_{5, 0, 0}], false, true);
    auto& c6 = make_chunk1(w[chunk_coords_{6, 0, 0}], false, false);
    auto& c7 = make_chunk1(w[chunk_coords_{7, 0, 0}], true, true);

    for (auto* cʹ : { &c1, &c2, &c3, &c4, &c5, &c6, &c7 })
        rebuild(*cʹ);

    Pass::Pool pool{Pass::Params{(uint32_t)state.range(0)}};
    pool.maybe_mark_stale_all(w.frame_no());
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

} // namespace

} // namespace floormat
