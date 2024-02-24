#include "src/chunk-region.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

chunk& make_chunk1(chunk& c, bool val, bool flipped)
{
    auto floor = tile_image_proto { loader.ground_atlas("texel"), 0 };
    auto empty = tile_image_proto{};

    constexpr uint8_t mat[TILE_MAX_DIM][TILE_MAX_DIM] = { // from test/region.cpp
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

void Chunk_Region(benchmark::State& state)
{
    auto w = world();
    auto& c1 = w[chunk_coords_{1, 0, 0}];
    auto& c2 = make_chunk1(w[chunk_coords_{2, 0, 0}], true, false);
    auto& c3 = make_chunk3(w[chunk_coords_{3, 0, 0}], false);
    auto& c4 = make_chunk3(w[chunk_coords_{4, 0, 0}], true);
    auto& c5 = make_chunk1(w[chunk_coords_{5, 0, 0}], false, true);
    auto& c6 = make_chunk1(w[chunk_coords_{6, 0, 0}], false, false);
    auto& c7 = make_chunk1(w[chunk_coords_{7, 0, 0}], true, true);
    chunk::pass_region p;

    for (auto _ : state)
    {
        { p = {}; c1.make_pass_region(p);
          auto cnt = p.bits.count();
          fm_assert(cnt == p.bits.size());
        }
        { p = {}; c2.make_pass_region(p);
          auto cnt = p.bits.count();
          fm_assert(cnt != 0 && cnt != p.bits.size());
        }
        { p = {}; c3.make_pass_region(p);
          auto cnt = p.bits.count();
          fm_assert(cnt == 0);
        }
        { p = {}; c4.make_pass_region(p);
          auto cnt = p.bits.count();
          fm_assert(cnt != 0 && cnt < 100);
        }
        { p = {}; c5.make_pass_region(p);
          auto cnt = p.bits.count();
          fm_assert(cnt != 0 && cnt != p.bits.size());
        }
        { p = {}; c6.make_pass_region(p);
          auto cnt = p.bits.count();
          fm_assert(cnt != 0 && cnt != p.bits.size());
        }
        { p = {}; c7.make_pass_region(p);
          auto cnt = p.bits.count();
          fm_assert(cnt != 0 && cnt != p.bits.size());
        }
    }
}

BENCHMARK(Chunk_Region)->Unit(benchmark::kMillisecond);

} // namespace

} // namespace floormat
