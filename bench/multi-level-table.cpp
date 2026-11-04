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

template class multi_level_table<u64, object_like_params>;
template class multi_level_table<u64, chunk_like_params>;

namespace {

using object_like = multi_level_table<u64, object_like_params>;
using chunk_like = multi_level_table<u64, chunk_like_params>;
using hash_map = gtl::flat_hash_map<u64, u64>;

constexpr u64 first_key = 1025;
constexpr u32 page_keys = 1u << 18;
constexpr u64 absent_key = u64{1} << 34;
constexpr u32 local_window = 4096;
constexpr u32 window_batch = 1024;

enum class pattern : u32 { seq, interleaved, random, local, miss_page, miss_slot, count, };
constexpr const char* pattern_names[] = { "seq", "interleaved", "random", "local", "miss_page", "miss_slot", };

u32 below(Random::xoshiro256starstar& rng, u32 n)
{
    return u32((Random::next(rng) >> 32) * n >> 32);
}

benchmark::Counter per_op(u32 n)
{
    return benchmark::Counter(n, benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
}

template<mlt_params P>
CORRADE_ALWAYS_INLINE u64 lookup(const multi_level_table<u64, P>& t, u64 key)
{
    const u64* s = t.find(key);
    return s ? *s : 0;
}

CORRADE_ALWAYS_INLINE u64 lookup(const hash_map& m, u64 key)
{
    const auto it = m.find(key);
    return it != m.end() ? it->second : 0;
}

template<mlt_params P>
CORRADE_ALWAYS_INLINE void put(multi_level_table<u64, P>& t, u64 key)
{
    const bool ok = t.insert(key, key);
    fm_assert(ok);
}

CORRADE_ALWAYS_INLINE void put(hash_map& m, u64 key)
{
    const bool ok = m.emplace(key, key).second;
    fm_assert(ok);
}

template<mlt_params P>
CORRADE_ALWAYS_INLINE u64 take(multi_level_table<u64, P>& t, u64 key)
{
    return t.erase(key);
}

CORRADE_ALWAYS_INLINE u64 take(hash_map& m, u64 key)
{
    return m.erase(key);
}

template<typename Table>
void MLT_Find(benchmark::State& state, pattern p)
{
    const auto n = u32{1} << state.range(0);
    Table t;
    for (u32 i = 0; i < n; i++)
        put(t, p == pattern::miss_slot ? first_key + 2*i : first_key + i);

    Array<u64> order{NoInit, n};
    Random::xoshiro256starstar rng;
    Random::seed(rng, n);
    switch (p)
    {
    case pattern::seq:
    case pattern::random:
        for (u32 j = 0; j < n; j++)
            order[j] = first_key + j;
        if (p == pattern::random)
            for (u32 j = n; j-- > 1; )
                std::swap(order[j], order[below(rng, j + 1)]);
        break;
    case pattern::interleaved: {
        // key j and j+1 are a page apart
        const u32 pages = n / page_keys;
        fm_assert(pages > 1);
        for (u32 j = 0; j < n; j++)
            order[j] = first_key + u64{j % pages} * page_keys + j / pages;
        break;
    }
    case pattern::local:
        for (u32 j = 0; j < n; j++)
            order[j] = first_key + ((j + below(rng, local_window)) & (n - 1));
        break;
    case pattern::miss_page:
        for (u32 j = 0; j < n; j++)
            order[j] = absent_key + j;
        break;
    case pattern::miss_slot:
        for (u32 j = 0; j < n; j++)
            order[j] = first_key + 2*j + 1;
        break;
    case pattern::count:
        fm_assert(false);
    }

    u64 sum = 0;
    for (u64 k : order)
        sum += lookup(t, k);
    for (auto _ : state)
    {
        for (u64 k : order)
            sum += lookup(t, k);
        benchmark::DoNotOptimize(sum);
    }
    state.counters["lookup"] = per_op(n);
}

template<typename Table>
void MLT_InsertClear(benchmark::State& state)
{
    const auto n = u32{1} << state.range(0);
    Table t;
    for (u32 i = 0; i < n; i++)
        put(t, first_key + i);
    t.clear();
    for (auto _ : state)
    {
        for (u32 i = 0; i < n; i++)
            put(t, first_key + i);
        t.clear();
    }
    state.counters["insert"] = per_op(n);
}

template<typename Table>
void MLT_Window(benchmark::State& state)
{
    const auto w = u64(state.range(0));
    Table t;
    u64 back = first_key, front = first_key, sum = 0;
    while (front < first_key + w)
        put(t, front++);
    for (u32 i = 0; i < window_batch; i++)
    {
        put(t, front++);
        sum += take(t, back++);
    }
    for (auto _ : state)
    {
        for (u32 i = 0; i < window_batch; i++)
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
    for (u32 i = 0; i < (u32)pattern::count; i++)
    {
        const auto p = pattern(i);
        auto* b = benchmark::RegisterBenchmark("MLT_Find/" + name + "/" + pattern_names[i], &MLT_Find<Table>, p);
        b->ArgName("log2n")->Unit(benchmark::kMicrosecond);
        for (i64 lg : {12, 16, 20, 24})
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
