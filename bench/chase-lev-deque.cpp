#include "compat/atomic.hpp"
#include "compat/chase-lev-deque.hpp"
#include <thread>
#include <vector>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

struct item { uint32_t value; };

struct alignas(64) counter { volatile uint64_t value = 0; };

using chase_lev = ChaseLevDeque<item>;

void process(const item* x, uint32_t work)
{
    uint32_t v = x->value;
    for (uint32_t i = 0; i < work; ++i)
    {
        v = v * 0x9e3779b1u + 1;
        benchmark::DoNotOptimize(v);
    }
    benchmark::DoNotOptimize(v);
}

template<typename D>
void Deque_PushTake(benchmark::State& state)
{
    const auto n = (uint32_t)state.range(0);
    std::vector<item> items(n);
    ChaseLevBufferPool pool;
    D d{pool};
    for (auto _ : state)
    {
        for (auto& x : items)
            d.push(&x);
        for (uint32_t i = 0; i < n; ++i)
            benchmark::DoNotOptimize(d.take());
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(Deque_PushTake, chase_lev)->ArgName("n")->Arg(1)->Arg(256)->Unit(benchmark::kNanosecond);

template<typename D>
void Deque_Grow(benchmark::State& state)
{
    const auto n = (uint32_t)state.range(0);
    std::vector<item> items(n);
    ChaseLevBufferPool pool;
    for (auto _ : state)
    {
        D d{pool, 1};
        for (auto& x : items)
            d.push(&x);
        for (uint32_t i = 0; i < n; ++i)
            benchmark::DoNotOptimize(d.take());
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(Deque_Grow, chase_lev)->ArgName("n")->Arg(65536)->Unit(benchmark::kMicrosecond);

// Thieves spin, so give each thread its own CPU.
template<typename D>
void Deque_Steal(benchmark::State& state)
{
    const auto thieves = (uint32_t)state.range(0), n = (uint32_t)state.range(1), work = (uint32_t)state.range(2);
    std::vector<item> items(n);
    std::vector<counter> stolen(thieves);
    ChaseLevBufferPool pool;
    D d{pool};
    volatile uint8_t stop = 0;

    std::vector<std::thread> threads;
    threads.reserve(thieves);
    for (uint32_t i = 0; i < thieves; ++i)
        threads.emplace_back([&, i] {
            uint64_t k = 0;
            while (!atomic_load(&stop, memory_order::relaxed))
            {
                if (item* x = d.steal())
                {
                    process(x, work);
                    atomic_store(&stolen[i].value, ++k, memory_order::release);
                }
                else
                    cpu_relax();
            }
        });

    uint64_t pushed = 0, taken = 0;
    for (auto _ : state)
    {
        for (auto& x : items)
            d.push(&x);
        pushed += n;
        while (item* x = d.take())
        {
            process(x, work);
            ++taken;
        }
        for (;;)
        {
            uint64_t sum = taken;
            for (auto& c : stolen)
                sum += atomic_load(&c.value, memory_order::acquire);
            if (sum == pushed)
                break;
            cpu_relax();
        }
    }

    atomic_store(&stop, uint8_t{1}, memory_order::relaxed);
    for (auto& t : threads)
        t.join();
    state.SetItemsProcessed(state.iterations() * n);
    state.counters["stolen"] = 1 - (double)taken / (double)pushed;
}
BENCHMARK_TEMPLATE(Deque_Steal, chase_lev)
    ->ArgsProduct({{0, 1, 3}, {64, 4096}, {0, 64, 1024}})
    ->ArgNames({"thieves", "n", "work"})
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);

} // namespace

} // namespace floormat
