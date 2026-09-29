#include "compat/borrowed-ptr.inl"
#include "compat/weak-borrowed-ptr.inl"
#include <algorithm>
#include <array>
#include <random>
#include <vector>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

struct Obj final : bptr_base { int val = 0; };

using fused = non_atomic_refcount;
struct separate : non_atomic_refcount {};

} // namespace

template<typename T>
struct bptr_traits<T, separate> : default_bptr_traits<T, separate>
{
    static constexpr size_t max_inplace_size = 0;
};

namespace {

static_assert(detail_bptr::stores_inplace<Obj, fused>);
static_assert(!detail_bptr::stores_inplace<Obj, separate>);

constexpr uint32_t block_count = 1024;

template<typename P>
void Bptr_Copy(benchmark::State& state)
{
    auto p = basic_bptr<Obj, P>{InPlace};
    for (auto _ : state)
    {
        auto q = p;
        benchmark::DoNotOptimize(q);
    }
}
BENCHMARK_TEMPLATE(Bptr_Copy, fused)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(Bptr_Copy, separate)->Unit(benchmark::kNanosecond);

template<typename P>
void Bptr_Copy_Many(benchmark::State& state)
{
    std::array<basic_bptr<Obj, P>, block_count> src, dst;
    for (auto& p : src)
        p = basic_bptr<Obj, P>{InPlace};
    for (auto _ : state)
    {
        for (auto i = 0u; i < block_count; i++)
            dst[i] = src[i];
        benchmark::ClobberMemory();
        for (auto& p : dst)
            p = nullptr;
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * block_count);
}
BENCHMARK_TEMPLATE(Bptr_Copy_Many, fused)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(Bptr_Copy_Many, separate)->Unit(benchmark::kMicrosecond);

template<typename P>
void Bptr_Make(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto p = basic_bptr<Obj, P>{InPlace};
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK_TEMPLATE(Bptr_Make, fused)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(Bptr_Make, separate)->Unit(benchmark::kNanosecond);

template<typename P>
void Bptr_Deref_Many(benchmark::State& state)
{
    constexpr uint32_t count = 1 << 21;
    std::vector<basic_bptr<Obj, P>> ptrs(count);
    for (auto& p : ptrs)
        p = basic_bptr<Obj, P>{InPlace};
    if (state.range(0))
        std::shuffle(ptrs.begin(), ptrs.end(), std::mt19937{42});
    for (auto _ : state)
    {
        int sum = 0;
        for (const auto& p : ptrs)
            sum += p->val;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK_TEMPLATE(Bptr_Deref_Many, fused)->Arg(0)->Arg(1)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(Bptr_Deref_Many, separate)->Arg(0)->Arg(1)->Unit(benchmark::kMillisecond);

template<typename P>
void Weak_Lock(benchmark::State& state)
{
    auto p = basic_bptr<Obj, P>{InPlace};
    auto w = basic_weak_bptr<Obj, P>{p};
    for (auto _ : state)
    {
        auto q = w.lock();
        benchmark::DoNotOptimize(q);
    }
}
BENCHMARK_TEMPLATE(Weak_Lock, fused)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(Weak_Lock, separate)->Unit(benchmark::kNanosecond);

template<typename P>
void Weak_Lock_Many(benchmark::State& state)
{
    std::array<basic_bptr<Obj, P>, block_count> src, dst;
    std::array<basic_weak_bptr<Obj, P>, block_count> weak;
    for (auto i = 0u; i < block_count; i++)
    {
        src[i] = basic_bptr<Obj, P>{InPlace};
        weak[i] = src[i];
    }
    for (auto _ : state)
    {
        for (auto i = 0u; i < block_count; i++)
            dst[i] = weak[i].lock();
        benchmark::ClobberMemory();
        for (auto& p : dst)
            p = nullptr;
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * block_count);
}
BENCHMARK_TEMPLATE(Weak_Lock_Many, fused)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(Weak_Lock_Many, separate)->Unit(benchmark::kMicrosecond);

template<typename P>
void Weak_Copy(benchmark::State& state)
{
    auto p = basic_bptr<Obj, P>{InPlace};
    auto w = basic_weak_bptr<Obj, P>{p};
    for (auto _ : state)
    {
        auto w2 = w;
        benchmark::DoNotOptimize(w2);
    }
}
BENCHMARK_TEMPLATE(Weak_Copy, fused)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(Weak_Copy, separate)->Unit(benchmark::kNanosecond);

} // namespace

} // namespace floormat
