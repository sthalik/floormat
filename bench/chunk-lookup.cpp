#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/global-coords.hpp"
#include "src/tile-defs.hpp"
#include <benchmark/benchmark.h>
#include <array>
#include <random>

namespace floormat {

namespace {

constexpr int rect_extent = 5;
constexpr int rect_dim = rect_extent * 2 + 1;
constexpr int rect_n = rect_dim * rect_dim;

constexpr size_t K = 100'000;

struct fixture
{
    world w;
    std::array<chunk_coords_, K> queries{};
    std::array<chunk*, rect_n> arr{};

    static size_t flat_index(chunk_coords_ ch)
    {
        auto x = (uint32_t)((int)ch.x + rect_extent);
        auto y = (uint32_t)((int)ch.y + rect_extent);
        return (size_t)y * (size_t)rect_dim + (size_t)x;
    }

    fixture()
    {
        for (int j = -rect_extent; j <= rect_extent; ++j)
            for (int i = -rect_extent; i <= rect_extent; ++i)
            {
                chunk_coords_ ch{(int16_t)i, (int16_t)j, 0};
                auto& c = w[ch];
                arr[flat_index(ch)] = &c;
            }

        std::mt19937 rng{0xc0ffeeu};
        std::uniform_int_distribution<int> dist{-rect_extent, rect_extent};
        for (auto& q : queries)
            q = {(int16_t)dist(rng), (int16_t)dist(rng), 0};
    }
};

void Lookup_Array(benchmark::State& state)
{
    fixture f;
    for (auto _ : state)
    {
        for (auto& q : f.queries)
        {
            chunk* c = f.arr[fixture::flat_index(q)];
            benchmark::DoNotOptimize(c);
        }
    }
    state.SetItemsProcessed(state.iterations() * (int64_t)K);
}

void Lookup_WorldAt(benchmark::State& state)
{
    fixture f;
    for (auto _ : state)
    {
        for (auto& q : f.queries)
        {
            chunk* c = f.w.at(q);
            benchmark::DoNotOptimize(c);
        }
    }
    state.SetItemsProcessed(state.iterations() * (int64_t)K);
}

void Lookup_ChunkAtMemo(benchmark::State& state)
{
    fixture f;
    for (auto& q : f.queries)
        (void)f.w.chunk_at_memo(q);
    f.w.chunk_memo_prepare_frame();
    for (auto& q : f.queries)
        (void)f.w.chunk_at_memo(q);

    for (auto _ : state)
    {
        for (auto& q : f.queries)
        {
            chunk* c = f.w.chunk_at_memo(q);
            benchmark::DoNotOptimize(c);
        }
    }
    state.SetItemsProcessed(state.iterations() * (int64_t)K);
}

} // namespace

BENCHMARK(Lookup_Array)->Unit(benchmark::kMicrosecond);
BENCHMARK(Lookup_WorldAt)->Unit(benchmark::kMicrosecond);
BENCHMARK(Lookup_ChunkAtMemo)->Unit(benchmark::kMicrosecond);

} // namespace floormat
