#include "compat/function-view.hpp"
#include "compat/inplace-function.hpp"
#include <utility>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

using inplace_fn = inplace_function<u64(u64), 40, 8>;

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
BENCHMARK_TEMPLATE(Function_MoveAssign, inplace_fn);

template<u32 N> struct signature_of;
template<> struct signature_of<1> { using type = u64(u64) const; };
template<> struct signature_of<3> { using type = u64(u64, u64, u64) const; };

template<typename Signature> using inplace_ref = const inplace_function<Signature, 16>&;

template<u32 N, typename F> u64 call_with(const F& f, u64 x, u64 y, u64 z)
{
    if constexpr (N == 1)
        return f(x);
    else
        return f(x, y, z);
}

template<typename W, u32 N> u64 call_64(W f, u64 x, u64 y, u64 z)
{
    for (u32 i = 0; i < 64; i++)
        x = call_with<N>(f, x, y, z);
    return x;
}

template<typename W, u32 N> u64 call_once(W f, u64 x, u64 y, u64 z)
{
    return call_with<N>(f, x, y, z);
}

auto make_callback(const u64& a, const u64& b)
{
    return [&a, &b](u64 x, auto... rest) { return x * a + (b ^ ... ^ rest); };
}

template<template<typename> class W, u32 N>
void Callback_Call(benchmark::State& state)
{
    using wrapper = W<typename signature_of<N>::type>;
    u64 a = opaque(3), b = opaque(5), y = opaque(7), z = opaque(11), x = 1;
    auto f = make_callback(a, b);
    std::remove_cvref_t<wrapper> w{f};
    u64 (*volatile call)(wrapper, u64, u64, u64) = &call_64<wrapper, N>;
    for (auto _ : state)
        x = call(w, x, y, z);
    benchmark::DoNotOptimize(x);
    state.SetItemsProcessed(state.iterations() * 64);
}
BENCHMARK_TEMPLATE(Callback_Call, function_view, 1);
BENCHMARK_TEMPLATE(Callback_Call, inplace_ref, 1);
BENCHMARK_TEMPLATE(Callback_Call, function_view, 3);
BENCHMARK_TEMPLATE(Callback_Call, inplace_ref, 3);

template<template<typename> class W, u32 N>
void Callback_ConstructCall(benchmark::State& state)
{
    using wrapper = W<typename signature_of<N>::type>;
    u64 a = opaque(3), b = opaque(5), y = opaque(7), z = opaque(11), x = 1;
    u64 (*volatile call)(wrapper, u64, u64, u64) = &call_once<wrapper, N>;
    for (auto _ : state)
    {
        auto f = make_callback(a, b);
        std::remove_cvref_t<wrapper> w{f};
        x = call(w, x, y, z);
    }
    benchmark::DoNotOptimize(x);
}
BENCHMARK_TEMPLATE(Callback_ConstructCall, function_view, 1);
BENCHMARK_TEMPLATE(Callback_ConstructCall, inplace_ref, 1);
BENCHMARK_TEMPLATE(Callback_ConstructCall, function_view, 3);
BENCHMARK_TEMPLATE(Callback_ConstructCall, inplace_ref, 3);

} // namespace

} // namespace floormat
