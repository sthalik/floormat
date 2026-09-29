#include "compat/multi-level-table.inl"
#include "random/random.hpp"
#include <cr/Array.h>
#include <gtl/phmap.hpp>
#include <benchmark/benchmark.h>
#include <string>
#include <utility>

namespace floormat {

namespace {

constexpr inline mlt_params object_like_params = mlt_params{
    .levels = { {.bits = 17}, {.bits = 18, .dynamic = true}, },
    .top_source = mlt_source::superpage,
    .page_source = mlt_source::superpage,
    .free_empty = true,
}.validate();

constexpr inline mlt_params chunk_like_params = mlt_params{
    .levels = { {.bits = 14}, {.bits = 4}, {.bits = 18, .dynamic = true}, },
    .top_source = mlt_source::superpage,
    .page_source = mlt_source::superpage,
}.validate();

} // namespace

template class multi_level_table<uint64_t, object_like_params>;
template class multi_level_table<uint64_t, chunk_like_params>;

namespace {

using object_like = multi_level_table<uint64_t, object_like_params>;
using chunk_like = multi_level_table<uint64_t, chunk_like_params>;
using hash_map = gtl::flat_hash_map<uint64_t, uint64_t>;

constexpr uint64_t first_key = 1025;
constexpr uint32_t page_keys = 1u << 18;
constexpr uint64_t absent_key = uint64_t{1} << 34;
constexpr uint32_t local_window = 4096;
constexpr uint32_t window_batch = 1024;

enum class pattern : uint32_t { seq, interleaved, random, local, miss_page, miss_slot, count, };
constexpr const char* pattern_names[] = { "seq", "interleaved", "random", "local", "miss_page", "miss_slot", };

uint32_t below(Random::xoshiro256starstar& rng, uint32_t n)
{
    return uint32_t((Random::next(rng) >> 32) * n >> 32);
}

benchmark::Counter per_op(uint32_t n)
{
    return benchmark::Counter(n, benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
}

template<mlt_params P>
CORRADE_ALWAYS_INLINE uint64_t lookup(const multi_level_table<uint64_t, P>& t, uint64_t key)
{
    const uint64_t* s = t.find(key);
    return s ? *s : 0;
}

CORRADE_ALWAYS_INLINE uint64_t lookup(const hash_map& m, uint64_t key)
{
    const auto it = m.find(key);
    return it != m.end() ? it->second : 0;
}

template<mlt_params P>
CORRADE_ALWAYS_INLINE void put(multi_level_table<uint64_t, P>& t, uint64_t key)
{
    const bool ok = t.insert(key, key);
    fm_assert(ok);
}

CORRADE_ALWAYS_INLINE void put(hash_map& m, uint64_t key)
{
    const bool ok = m.emplace(key, key).second;
    fm_assert(ok);
}

template<mlt_params P>
CORRADE_ALWAYS_INLINE uint64_t take(multi_level_table<uint64_t, P>& t, uint64_t key)
{
    return t.erase(key);
}

CORRADE_ALWAYS_INLINE uint64_t take(hash_map& m, uint64_t key)
{
    return m.erase(key);
}

template<typename Table>
void MLT_Find(benchmark::State& state, pattern p)
{
    const auto n = uint32_t{1} << state.range(0);
    Table t;
    for (uint32_t i = 0; i < n; i++)
        put(t, p == pattern::miss_slot ? first_key + 2*i : first_key + i);

    Array<uint64_t> order{NoInit, n};
    Random::xoshiro256starstar rng;
    Random::seed(rng, n);
    switch (p)
    {
    case pattern::seq:
    case pattern::random:
        for (uint32_t j = 0; j < n; j++)
            order[j] = first_key + j;
        if (p == pattern::random)
            for (uint32_t j = n; j-- > 1; )
                std::swap(order[j], order[below(rng, j + 1)]);
        break;
    case pattern::interleaved: {
        // key j and j+1 are a page apart
        const uint32_t pages = n / page_keys;
        fm_assert(pages > 1);
        for (uint32_t j = 0; j < n; j++)
            order[j] = first_key + uint64_t{j % pages} * page_keys + j / pages;
        break;
    }
    case pattern::local:
        for (uint32_t j = 0; j < n; j++)
            order[j] = first_key + ((j + below(rng, local_window)) & (n - 1));
        break;
    case pattern::miss_page:
        for (uint32_t j = 0; j < n; j++)
            order[j] = absent_key + j;
        break;
    case pattern::miss_slot:
        for (uint32_t j = 0; j < n; j++)
            order[j] = first_key + 2*j + 1;
        break;
    case pattern::count:
        fm_assert(false);
    }

    uint64_t sum = 0;
    for (uint64_t k : order)
        sum += lookup(t, k);
    for (auto _ : state)
    {
        for (uint64_t k : order)
            sum += lookup(t, k);
        benchmark::DoNotOptimize(sum);
    }
    state.counters["lookup"] = per_op(n);
}

template<typename Table>
void MLT_InsertClear(benchmark::State& state)
{
    const auto n = uint32_t{1} << state.range(0);
    Table t;
    for (uint32_t i = 0; i < n; i++)
        put(t, first_key + i);
    t.clear();
    for (auto _ : state)
    {
        for (uint32_t i = 0; i < n; i++)
            put(t, first_key + i);
        t.clear();
    }
    state.counters["insert"] = per_op(n);
}

template<typename Table>
void MLT_Window(benchmark::State& state)
{
    const auto w = uint64_t(state.range(0));
    Table t;
    uint64_t back = first_key, front = first_key, sum = 0;
    while (front < first_key + w)
        put(t, front++);
    for (uint32_t i = 0; i < window_batch; i++)
    {
        put(t, front++);
        sum += take(t, back++);
    }
    for (auto _ : state)
    {
        for (uint32_t i = 0; i < window_batch; i++)
        {
            put(t, front++);
            sum += take(t, back++);
        }
        benchmark::DoNotOptimize(sum);
    }
    state.counters["step"] = per_op(window_batch);
}

template<typename Table>
void register_table(const std::string& name)
{
    for (uint32_t i = 0; i < (uint32_t)pattern::count; i++)
    {
        const auto p = pattern(i);
        auto* b = benchmark::RegisterBenchmark("MLT_Find/" + name + "/" + pattern_names[i], &MLT_Find<Table>, p);
        b->ArgName("log2n")->Unit(benchmark::kMicrosecond);
        for (int64_t lg : {12, 16, 20, 24})
            if (p != pattern::interleaved || lg >= 20)
                b->Arg(lg);
    }
    benchmark::RegisterBenchmark("MLT_InsertClear/" + name, &MLT_InsertClear<Table>)
        ->ArgName("log2n")->Arg(12)->Arg(16)->Arg(20)->Arg(24)->Unit(benchmark::kMicrosecond);
}

[[maybe_unused]] const int registered = [] {
    register_table<object_like>("object_like");
    register_table<chunk_like>("chunk_like");
    register_table<hash_map>("gtl_flat_hash_map");
    benchmark::RegisterBenchmark("MLT_Window/object_like", &MLT_Window<object_like>)
        ->ArgName("w")->Arg(64)->Arg(65536)->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("MLT_Window/gtl_flat_hash_map", &MLT_Window<hash_map>)
        ->ArgName("w")->Arg(64)->Arg(65536)->Unit(benchmark::kMicrosecond);
    return 0;
}();

} // namespace

} // namespace floormat
