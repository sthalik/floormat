#include "compat/function2.hpp"
#include "compat/inplace-function.hpp"
#include <utility>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

using fu2_fn = fu2::function_base<true, false, fu2::capacity_fixed<40, 8>, false, false, u64(u64)>;
using inplace_fn = inplace_function<u64(u64), 40, 8>;
static_assert(sizeof(fu2_fn) == sizeof(inplace_fn));

auto make_target(u64 k)
{
    return [k, j = k * 3, m = k ^ 0x9e3779b1u](u64 x) noexcept { return x * k + (j ^ m); };
}

u64 opaque(u64 k)
{
    benchmark::DoNotOptimize(k);
    return k;
}

// DoNotOptimize(f) makes each call reload the target pointer, as a call through a queue slot does.
template<typename Fn>
void Function_Call(benchmark::State& state)
{
    Fn f{make_target(opaque(3))};
    u64 x = 1;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(f);
        x = f(x);
    }
    u64 result = x;
    benchmark::DoNotOptimize(result);
}
BENCHMARK_TEMPLATE(Function_Call, fu2_fn);
BENCHMARK_TEMPLATE(Function_Call, inplace_fn);

template<typename Fn>
void Function_ConstructDestroy(benchmark::State& state)
{
    u64 k = 3;
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(k);
        Fn f{make_target(k)};
        benchmark::DoNotOptimize(f);
    }
}
BENCHMARK_TEMPLATE(Function_ConstructDestroy, fu2_fn);
BENCHMARK_TEMPLATE(Function_ConstructDestroy, inplace_fn);

template<typename Fn>
void Function_MoveConstruct(benchmark::State& state)
{
    union slot { Fn f; slot() {} ~slot() {} };
    slot a, b;
    Fn* from = std::construct_at(&a.f, make_target(opaque(3)));
    Fn* to = &b.f;
    for (auto _ : state)
    {
        std::construct_at(to, std::move(*from));
        std::destroy_at(from);
        benchmark::DoNotOptimize(*to);
        std::swap(from, to);
    }
    std::destroy_at(from);
}
BENCHMARK_TEMPLATE(Function_MoveConstruct, fu2_fn);
BENCHMARK_TEMPLATE(Function_MoveConstruct, inplace_fn);

template<typename Fn>
void Function_MoveAssign(benchmark::State& state)
{
    Fn a{make_target(opaque(3))}, b;
    Fn *from = &a, *to = &b;
    for (auto _ : state)
    {
        *to = std::move(*from);
        benchmark::DoNotOptimize(*to);
        std::swap(from, to);
    }
}
BENCHMARK_TEMPLATE(Function_MoveAssign, fu2_fn);
BENCHMARK_TEMPLATE(Function_MoveAssign, inplace_fn);

} // namespace

} // namespace floormat
