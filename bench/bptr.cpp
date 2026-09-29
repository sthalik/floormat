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

struct fused {};
struct separate {};

template<typename L>
bptr<Obj> make()
{
    if constexpr (std::is_same_v<L, separate>)
        return bptr<Obj>{new Obj};
    else
        return bptr<Obj>{InPlace};
}

constexpr uint32_t block_count = 1024;

template<typename L>
void Bptr_Copy(benchmark::State& state)
{
    auto p = make<L>();
    for (auto _ : state)
    {
        auto q = p;
        benchmark::DoNotOptimize(q);
    }
}
BENCHMARK_TEMPLATE(Bptr_Copy, fused)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(Bptr_Copy, separate)->Unit(benchmark::kNanosecond);

template<typename L>
void Bptr_Copy_Many(benchmark::State& state)
{
    std::array<bptr<Obj>, block_count> src, dst;
    for (auto& p : src)
        p = make<L>();
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

template<typename L>
void Bptr_Make(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto p = make<L>();
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK_TEMPLATE(Bptr_Make, fused)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(Bptr_Make, separate)->Unit(benchmark::kNanosecond);

template<typename L>
void Bptr_Deref_Many(benchmark::State& state)
{
    constexpr uint32_t count = 1 << 21;
    std::vector<bptr<Obj>> ptrs(count);
    for (auto& p : ptrs)
        p = make<L>();
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

template<typename L>
void Weak_Lock(benchmark::State& state)
{
    auto p = make<L>();
    auto w = weak_bptr<Obj>{p};
    for (auto _ : state)
    {
        auto q = w.lock();
        benchmark::DoNotOptimize(q);
    }
}
BENCHMARK_TEMPLATE(Weak_Lock, fused)->Unit(benchmark::kNanosecond);
BENCHMARK_TEMPLATE(Weak_Lock, separate)->Unit(benchmark::kNanosecond);

template<typename L>
void Weak_Lock_Many(benchmark::State& state)
{
    std::array<bptr<Obj>, block_count> src, dst;
    std::array<weak_bptr<Obj>, block_count> weak;
    for (auto i = 0u; i < block_count; i++)
    {
        src[i] = make<L>();
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

template<typename L>
void Weak_Copy(benchmark::State& state)
{
    auto p = make<L>();
    auto w = weak_bptr<Obj>{p};
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
