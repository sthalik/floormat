#include <benchmark/benchmark.h>

#if 0
namespace {

void noop(benchmark::State& state)
{
    for (auto _ : state)
        (void)0;
}

BENCHMARK(noop);

} // namespace
#endif
