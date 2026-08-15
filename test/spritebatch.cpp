#include "app.hpp"
#include "src/spritebatch.hpp"
#include "src/quads.hpp"
#include <vector>
#include <bit>
#include <cfloat>
#include <utility>
#include <algorithm>
#include <cr/ArrayView.h>

namespace floormat::Test {

namespace {

const Quads::vertexes dummy_quad{};

// Sortedness and permutation are both satisfied by garbage when depths tie, so the third
// check is what pins the merge down: a run's elements must keep their relative order.
struct fixture
{
    SpriteBatch sb;
    std::vector<float> depths;      // by emission index
    std::vector<uint32_t> run_of;   // by emission index
    std::vector<uint32_t> rank_of;  // by emission index, ascending depth within its run
    uint32_t nruns = 0;
    bool ranks_defined = true;      // false once some run has a repeated depth

    // Emitted in descending depth, so end_chunk(true) has real work to do.
    void add_run(std::vector<float> run)
    {
        std::ranges::sort(run, std::greater<>{});
        emit_run(run, true);
    }

    // Already ascending and emitted with end_chunk(false) — the static scenery path, where
    // the run is sorted at build time and SpriteBatch is told to trust it.
    void add_presorted_run(std::vector<float> run)
    {
        std::ranges::sort(run);
        emit_run(run, false);
    }

    void add_empty_run(bool do_sort)
    {
        sb.begin_chunk();
        sb.end_chunk(do_sort);
    }

    void emit_run(const std::vector<float>& run, bool do_sort)
    {
        const auto first = (uint32_t)depths.size();
        sb.begin_chunk();
        for (float d : run)
        {
            sb.emit(dummy_quad, d);
            depths.push_back(d);
            run_of.push_back(nruns);
            rank_of.push_back(0);
        }
        sb.end_chunk(do_sort);

        // Rank by ascending depth within the run. ranges::sort is not stable, so a repeated
        // depth leaves the post-sort order unspecified and ranks become meaningless.
        std::vector<uint32_t> by_depth;
        for (auto i = first; i < (uint32_t)depths.size(); i++)
            by_depth.push_back(i);
        std::ranges::sort(by_depth, [&](uint32_t a, uint32_t b) { return depths[a] < depths[b]; });
        for (auto i = 0u; i < by_depth.size(); i++)
        {
            rank_of[by_depth[i]] = i;
            if (i > 0 && depths[by_depth[i-1]] == depths[by_depth[i]])
                ranks_defined = false;
        }
        if (!run.empty())
            nruns++;
    }

    void check(bool do_sort = true)
    {
        sb.sort_vertex_buffer(do_sort);
        const auto order = sb.merged_order();
        fm_assert(order.size() == depths.size());

        std::vector<bool> seen(depths.size(), false);
        std::vector<uint32_t> next_rank(nruns, 0);

        for (auto i = 0uz; i < order.size(); i++)
        {
            const auto j = order[i];
            fm_assert(j < depths.size());
            fm_assert(!seen[j]); // a repeated index means some other element was dropped
            seen[j] = true;

            if (do_sort && i > 0)
                fm_assert(depths[order[i-1]] <= depths[j]);

            if (do_sort && ranks_defined)
            {
                // Runs interleave, but each one must be consumed front to back.
                auto& r = next_rank[run_of[j]];
                fm_assert(rank_of[j] == r);
                r++;
            }
        }
        for (auto b : seen)
            fm_assert(b);
        reset();
    }

    // Every run emitted with end_chunk(false) leaves sort_indexes untouched. That identity
    // is what draw()'s direct path relies on when it uploads impl.verts unpermuted.
    void check_identity()
    {
        sb.sort_vertex_buffer(false);
        const auto order = sb.merged_order();
        fm_assert(order.size() == depths.size());
        for (auto i = 0u; i < order.size(); i++)
            fm_assert(order[i] == i);
        reset();
    }

    void reset()
    {
        sb.clear();
        depths.clear();
        run_of.clear();
        rank_of.clear();
        nruns = 0;
        ranks_defined = true;
    }
};

// Every strictly increasing run of length 1..max_len over {0 .. alphabet-1}. Strictly
// increasing keeps within-run ranks well defined; ties across runs are what the merge
// has to get right, and the k-tuple enumeration below covers those exhaustively.
std::vector<std::vector<float>> enumerate_runs(uint32_t alphabet, uint32_t max_len)
{
    std::vector<std::vector<float>> out;
    for (uint32_t mask = 1; mask < 1u << alphabet; mask++)
    {
        if ((uint32_t)std::popcount(mask) > max_len)
            continue;
        std::vector<float> v;
        for (auto i = 0u; i < alphabet; i++)
            if (mask & 1u << i)
                v.push_back((float)i);
        out.push_back(std::move(v));
    }
    return out;
}

void test_exhaustive(uint32_t k, uint32_t alphabet, uint32_t max_len)
{
    const auto runs = enumerate_runs(alphabet, max_len);
    const auto n = (uint32_t)runs.size();
    std::vector<uint32_t> sel(k, 0);
    fixture f;

    for (;;)
    {
        for (auto r : sel)
            f.add_run(runs[r]);
        f.check();

        uint32_t i = 0;
        for (; i < k; i++)
        {
            if (++sel[i] < n)
                break;
            sel[i] = 0;
        }
        if (i == k)
            return;
    }
}

// Distinct primes, so no two runs exhaust on the same iteration.
constexpr inline uint32_t run_lengths[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41 };

// Both extremes of the streak fast path have to run: interleaved never takes it, blocked
// takes it for all but k of the pops.
enum class layout : uint8_t { interleaved, blocked, one_dominant, reverse_blocked };

std::vector<float> make_run(layout kind, uint32_t run, uint32_t len)
{
    std::vector<float> v;
    v.reserve(len);
    for (auto i = 0u; i < len; i++)
        switch (kind)
        {
        case layout::interleaved:     v.push_back((float)(i * 64 + run)); break;
        case layout::blocked:         v.push_back((float)(run * 4096 + i)); break;
        // Runs exhaust in the reverse of tree order, so the sentinel propagates up from
        // the last leaf rather than the first.
        case layout::reverse_blocked: v.push_back((float)((64 - run) * 4096 + i)); break;
        // One dense run spanning everything, the rest sparse — the clustered-scenery case.
        case layout::one_dominant:    v.push_back(run == 0 ? (float)i : (float)(i * 512 + run)); break;
        }
    return v;
}

void test_layout(layout kind, uint32_t k)
{
    fixture f;
    for (auto i = 0u; i < k; i++)
        f.add_run(make_run(kind, i, run_lengths[i % (uint32_t)std::size(run_lengths)]));
    f.check();
}

} // namespace

void test_spritebatch()
{
    // Every cross-run tie pattern for small k. 14 distinct runs at k <= 4, 6 at k = 5.
    for (auto k : { 2u, 3u, 4u })
        test_exhaustive(k, 4, 3);
    test_exhaustive(5, 3, 2);

    // The ascent is `p = (k+i)/2; p /= 2`, so leaves sit at different depths depending on
    // where k falls relative to a power of two. Straddle every boundary up to 64.
    for (auto k : { 2u, 3u, 4u, 5u, 7u, 8u, 9u, 15u, 16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u })
        for (auto kind : { layout::interleaved, layout::blocked, layout::one_dominant, layout::reverse_blocked })
            test_layout(kind, k);

    {   // k == 1 skips the merge; k == 0 means every chunk was empty.
        fixture f;
        f.add_run({3, 1, 2});
        f.check();
        f.add_empty_run(true);
        f.add_empty_run(false);
        f.check();
    }

    {   // Empty chunks between real ones must not open a run, or the terminator lands in
        // the wrong slot and every subsequent run reads the wrong bounds.
        fixture f;
        f.add_empty_run(true);
        f.add_run({5, 1});
        f.add_empty_run(true);
        f.add_empty_run(false);
        f.add_run({4, 2});
        f.add_empty_run(false);
        f.check();
    }

    {   // A run of length 1 exhausts on its first pop, which is where head[] must go to
        // FLT_MAX rather than reading past the run.
        fixture f;
        f.add_run({5});
        f.add_run({1, 2, 3});
        f.add_run({4});
        f.check();
    }

    {   // All runs length 1: every pop exhausts a run, so the tree is rebuilt from
        // sentinels on every iteration.
        fixture f;
        for (auto i = 0u; i < 17; i++)
            f.add_run({(float)(17 - i)});
        f.check();
    }

    {   // Every element equal. The fast path fires on ties, so a wrong `second` emits past
        // the end of a run here. Within-run ranks are undefined, so only permutation and
        // sortedness are checked.
        fixture f;
        for (auto i = 0u; i < 6; i++)
            f.add_run(std::vector<float>(7, 42));
        fm_assert(!f.ranks_defined);
        f.check();
    }

    {   // Ties across runs only. Ranks stay defined, so this catches a merge that reorders
        // within a run while still emitting a sorted permutation.
        fixture f;
        for (auto i = 0u; i < 5; i++)
            f.add_run({1, 2, 3, 4});
        fm_assert(f.ranks_defined);
        f.check();
    }

    {   // The mixed scenery pass: dynamic runs sorted by end_chunk, static runs pre-sorted
        // and passed through. Both kinds feed the same merge.
        fixture f;
        f.add_run({9, 3, 6});
        f.add_presorted_run({1, 4, 7});
        f.add_run({8, 2});
        f.add_presorted_run({0, 5});
        f.check();
    }

    {   // do_sort = false at the batch level: vertices go out in input order regardless of
        // run count, and sort_indexes is left alone.
        fixture f;
        f.add_presorted_run({1, 2});
        f.add_presorted_run({3, 4});
        f.check_identity();

        // Same, but a sorted run in the middle permutes sort_indexes, so the direct path
        // must not be taken. Only sortedness is meaningless here, not the permutation.
        f.add_presorted_run({1, 2});
        f.add_run({9, 8});
        f.check(false);
    }

    {   // Negative and denormal depths — the sentinels are ±FLT_MAX, so nothing here may
        // collide with them.
        fixture f;
        f.add_run({-1e30f, -1, 0});
        f.add_run({-1e-30f, 1e-30f, 1e30f});
        f.add_run({-FLT_MAX/2, FLT_MAX/2});
        f.check();
    }
}

} // namespace floormat::Test
