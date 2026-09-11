#include "src/spritebatch.hpp"
#include "src/quads.hpp"
#include <vector>
#include <random>
#include <algorithm>
#include <cr/ArrayView.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

constexpr inline uint32_t nquads = 1u << 16;

const Quads::vertexes dummy_quad{};

enum class layout : uint8_t { blocked, interleaved, shuffled };

// Every layout produces k ascending runs of the same length, so the merge does the same
// number of extractions either way. What differs is how often the winning run changes,
// which is the only input to the loser tree's branches.
std::vector<std::vector<float>> make_runs(uint32_t k, layout l)
{
    const auto m = nquads / k;
    auto runs = std::vector<std::vector<float>>(k, std::vector<float>(m));

    switch (l)
    {
    case layout::blocked:
        // Run r owns [r*m, (r+1)*m), so the merge drains one run to exhaustion before
        // touching the next.
        for (auto r = 0u; r < k; r++)
            for (auto i = 0u; i < m; i++)
                runs[r][i] = (float)(r*m + i);
        break;
    case layout::interleaved:
        // Strict round robin. The winner changes on every single extraction.
        for (auto r = 0u; r < k; r++)
            for (auto i = 0u; i < m; i++)
                runs[r][i] = (float)(i*k + r);
        break;
    case layout::shuffled:
        {
            auto rng = std::mt19937{0x9e3779b9};
            auto dist = std::uniform_real_distribution<float>{0, (float)nquads};
            for (auto r = 0u; r < k; r++)
            {
                for (auto i = 0u; i < m; i++)
                    runs[r][i] = dist(rng);
                std::ranges::sort(runs[r]);
            }
        }
        break;
    }
    return runs;
}

// Fraction of extractions whose winner equals the previous one -- the `head[w] <= second`
// fast path in sort_vertex_buffer, and the thing the three layouts exist to vary.
double streak_rate(const std::vector<std::vector<float>>& runs)
{
    const auto k = (uint32_t)runs.size();
    auto pos = std::vector<uint32_t>(k, 0);
    uint32_t prev = (uint32_t)-1, total = 0, same = 0;

    for (;;)
    {
        uint32_t w = (uint32_t)-1;
        float best = 0;
        for (auto r = 0u; r < k; r++)
        {
            if (pos[r] == runs[r].size())
                continue;
            if (w == (uint32_t)-1 || runs[r][pos[r]] < best)
            {
                w = r;
                best = runs[r][pos[r]];
            }
        }
        if (w == (uint32_t)-1)
            break;
        pos[w]++;
        if (w == prev)
            same++;
        prev = w;
        total++;
    }
    return total ? (double)same / total : 0;
}

void refill(SpriteBatch& sb, const std::vector<std::vector<float>>& runs)
{
    sb.clear();
    for (const auto& run : runs)
    {
        sb.begin_chunk();
        for (float d : run)
            sb.emit(dummy_quad, d);
        sb.end_chunk(false); // already ascending, so no per-run sort in the timed path
    }
}

void run(benchmark::State& state, layout l, bool do_sort)
{
    const auto runs = make_runs((uint32_t)state.range(0), l);
    auto sb = SpriteBatch{};

    for (auto _ : state)
    {
        state.PauseTiming();
        refill(sb, runs);
        state.ResumeTiming();

        sb.sort_vertex_buffer(do_sort);
        benchmark::DoNotOptimize(sb.merged_order().data());
    }
    state.counters["streak"] = streak_rate(runs);
}

void SpriteBatch_Merge_Blocked(benchmark::State& state)
{
    run(state, layout::blocked, true);
}

void SpriteBatch_Merge_Interleaved(benchmark::State& state)
{
    run(state, layout::interleaved, true);
}

void SpriteBatch_Merge_Shuffled(benchmark::State& state)
{
    run(state, layout::shuffled, true);
}

// do_sort=false takes the early-out copy in sort_vertex_buffer, so this is the floor the
// three above are measured against.
void SpriteBatch_Merge_Skipped(benchmark::State& state)
{
    run(state, layout::shuffled, false);
}

BENCHMARK(SpriteBatch_Merge_Blocked)->Arg(4)->Arg(16)->Arg(64)->Unit(benchmark::kMicrosecond);
BENCHMARK(SpriteBatch_Merge_Interleaved)->Arg(4)->Arg(16)->Arg(64)->Unit(benchmark::kMicrosecond);
BENCHMARK(SpriteBatch_Merge_Shuffled)->Arg(4)->Arg(16)->Arg(64)->Unit(benchmark::kMicrosecond);
BENCHMARK(SpriteBatch_Merge_Skipped)->Arg(4)->Arg(16)->Arg(64)->Unit(benchmark::kMicrosecond);

} // namespace

} // namespace floormat
