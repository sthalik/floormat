#include "compat/assert.hpp"
#include "compat/thread-pool.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <vector>
#include <benchmark/benchmark.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sched.h>
#include <time.h>
#endif

namespace floormat {

namespace {

using namespace std::chrono_literals;
using std::chrono::steady_clock;

// compat/cpu-topology.hpp's set_current_thread_affinity and process_affinity_mask will replace these two.
std::vector<u32> allowed_cpus()
{
    std::vector<u32> cpus;
#ifdef _WIN32
    DWORD_PTR process_mask = 0, system_mask = 0;
    fm_assert(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask));
    for (u32 i = 0; i < sizeof(process_mask) * 8; ++i)
        if (process_mask >> i & 1)
            cpus.push_back(i);
#else
    cpu_set_t set;
    CPU_ZERO(&set);
    fm_assert(sched_getaffinity(0, sizeof(set), &set) == 0);
    for (u32 i = 0; i < CPU_SETSIZE; ++i)
        if (CPU_ISSET(i, &set))
            cpus.push_back(i);
#endif
    return cpus;
}

void set_current_thread_affinity(const std::vector<u32>& cpus)
{
#ifdef _WIN32
    DWORD_PTR mask = 0;
    for (u32 i : cpus)
        mask |= DWORD_PTR{1} << i;
    fm_assert(SetThreadAffinityMask(GetCurrentThread(), mask));
#else
    cpu_set_t set;
    CPU_ZERO(&set);
    for (u32 i : cpus)
        CPU_SET(i, &set);
    fm_assert(sched_setaffinity(0, sizeof(set), &set) == 0);
#endif
}

u64 process_cpu_time_ns()
{
#ifdef _WIN32
    FILETIME creation_time, exit_time, kernel_time, user_time;
    fm_assert(GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time));
    auto to_u64 = [](FILETIME t) { return (u64)t.dwHighDateTime << 32 | t.dwLowDateTime; };
    return (to_u64(kernel_time) + to_u64(user_time)) * 100;
#else
    timespec ts;
    fm_assert(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0);
    return (u64)ts.tv_sec * 1000000000 + (u64)ts.tv_nsec;
#endif
}

void burn_cpu_for(std::chrono::nanoseconds duration)
{
    const auto deadline = steady_clock::now() + duration;
    while (steady_clock::now() < deadline)
    {
    }
}

// Runs fn(pool) with the calling thread on the lowest allowed logical processor and worker i on the i-th one
// after it, then lets the calling thread run on all of them again.
template<typename F>
void run_on_pinned_pool(benchmark::State& state, thread_pool_options options, F&& fn)
{
    const std::vector<u32> cpus = allowed_cpus();
    if (cpus.size() < options.num_threads)
        state.SkipWithError("the process may run on fewer logical processors than the pool has threads");
    else
    {
        set_current_thread_affinity({cpus[0]});
        options.start_handler = [&cpus](u32 worker_index) noexcept {
            set_current_thread_affinity({cpus[worker_index]});
        };
        {
            ThreadPool pool{move(options)};
            fn(pool);
        }
        set_current_thread_affinity(cpus);
    }
}

double to_seconds(steady_clock::duration t)
{
    return std::chrono::duration<double>(t).count();
}

u64 to_nanoseconds(steady_clock::duration t)
{
    return (u64)std::chrono::duration_cast<std::chrono::nanoseconds>(t).count();
}

// Sorts times_ns.
void set_time_counters(benchmark::State& state, std::vector<u64>& times_ns)
{
    if (times_ns.empty())
        return;
    std::sort(times_ns.begin(), times_ns.end());
    const usize n = times_ns.size();
    state.counters["min_ns"] = (double)times_ns[0];
    state.counters["median_ns"] = (double)times_ns[n / 2];
    state.counters["p99_ns"] = (double)times_ns[(n * 99 + 99) / 100 - 1];
    state.counters["max_ns"] = (double)times_ns[n - 1];
}

void ThreadPool_Wake(benchmark::State& state)
{
    const auto idle_spin_usec = (u32)state.range(0);
    const auto pause = std::chrono::microseconds{state.range(1)};
    thread_pool_options options{.num_threads = 2, .is_caller_running_jobs = false, .idle_spin_usec = idle_spin_usec};
    run_on_pinned_pool(state, move(options), [&](ThreadPool& pool) {
        auto call = [&] {
            pool.parallel_for(1, 1, [](u32, u32, u32) { burn_cpu_for(1us); });
        };
        call();
        std::vector<u64> times_ns;
        times_ns.reserve((usize)state.max_iterations);
        u64 num_calls_finding_worker_waiting = 0;
        for (auto _ : state)
        {
            if (pause != 0us)
                std::this_thread::sleep_for(pause);
            num_calls_finding_worker_waiting += pool.num_waiting_workers();
            const auto t0 = steady_clock::now();
            call();
            const auto t = steady_clock::now() - t0;
            state.SetIterationTime(to_seconds(t));
            times_ns.push_back(to_nanoseconds(t));
        }
        set_time_counters(state, times_ns);
        state.counters["worker_waiting_fraction"] = (double)num_calls_finding_worker_waiting / (double)times_ns.size();
    });
}
BENCHMARK(ThreadPool_Wake)
    ->ArgsProduct({{0, 20, 200}, {0, 100, 2000}})
    ->ArgNames({"spin_us", "pause_us"})
    ->UseManualTime()
    ->Iterations(2000)
    ->Unit(benchmark::kMicrosecond);

void ThreadPool_Phase(benchmark::State& state)
{
    const auto idle_spin_usec = (u32)state.range(0);
    run_on_pinned_pool(state, {.num_threads = 2, .idle_spin_usec = idle_spin_usec}, [&](ThreadPool& pool) {
        auto run_phase = [&] {
            for (u32 i = 0; i < 5; ++i)
                pool.parallel_for(16, 1, [](u32, u32, u32) { burn_cpu_for(20us); });
        };
        run_phase();
        std::vector<u64> times_ns;
        times_ns.reserve((usize)state.max_iterations);
        const u64 cpu_time_start_ns = process_cpu_time_ns();
        for (auto _ : state)
        {
            const auto t0 = steady_clock::now();
            run_phase();
            const auto t = steady_clock::now() - t0;
            state.SetIterationTime(to_seconds(t));
            times_ns.push_back(to_nanoseconds(t));
        }
        const u64 cpu_time_ns = process_cpu_time_ns() - cpu_time_start_ns;
        set_time_counters(state, times_ns);
        state.counters["cpu_ns_per_iteration"] = (double)cpu_time_ns / (double)times_ns.size();
    });
}
BENCHMARK(ThreadPool_Phase)
    ->ArgName("spin_us")
    ->Arg(0)->Arg(1)->Arg(4)->Arg(16)->Arg(64)
    ->UseManualTime()
    ->Iterations(2000)
    ->Unit(benchmark::kMicrosecond);

constexpr u32 num_uneven_jobs = 1152;

// 1100 jobs of 5 us, 45 of 700 us and 7 of 10 ms, shuffled with a fixed seed.
const std::array<u32, num_uneven_jobs>& uneven_job_costs_usec()
{
    static const auto costs = [] {
        std::array<u32, num_uneven_jobs> a;
        for (u32 i = 0; i < num_uneven_jobs; ++i)
            a[i] = i < 1100 ? 5 : i < 1145 ? 700 : 10000;
        u32 x = 2463534242u;
        for (u32 i = num_uneven_jobs - 1; i > 0; --i)
        {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            std::swap(a[i], a[x % (i + 1)]);
        }
        return a;
    }();
    return costs;
}

// A grain size of 0 stands for default_grain_size(num_uneven_jobs).
void ThreadPool_UnevenJobs(benchmark::State& state)
{
    const auto& costs = uneven_job_costs_usec();
    u64 total_cost_usec = 0;
    for (u32 c : costs)
        total_cost_usec += c;
    run_on_pinned_pool(state, {.num_threads = 2}, [&](ThreadPool& pool) {
        const u32 grain_size = state.range(0) != 0 ? (u32)state.range(0) : pool.default_grain_size(num_uneven_jobs);
        auto call = [&] {
            pool.parallel_for(num_uneven_jobs, grain_size, [&](u32 begin, u32 end, u32) {
                for (u32 i = begin; i < end; ++i)
                    burn_cpu_for(std::chrono::microseconds{costs[i]});
            });
        };
        call();
        std::vector<u64> times_ns;
        times_ns.reserve((usize)state.max_iterations);
        for (auto _ : state)
        {
            const auto t0 = steady_clock::now();
            call();
            const auto t = steady_clock::now() - t0;
            state.SetIterationTime(to_seconds(t));
            times_ns.push_back(to_nanoseconds(t));
        }
        set_time_counters(state, times_ns);
        state.counters["grain_size"] = grain_size;
        const double half_total_cost_ns = (double)total_cost_usec * 1000 / 2;
        state.counters["median_over_half_cost"] = (double)times_ns[times_ns.size() / 2] / half_total_cost_ns;
    });
}
BENCHMARK(ThreadPool_UnevenJobs)
    ->ArgName("grain")
    ->Arg(1)->Arg(8)->Arg(18)->Arg(64)->Arg(144)->Arg(0)
    ->UseManualTime()
    ->Iterations(30)
    ->Unit(benchmark::kMillisecond);

} // namespace

} // namespace floormat
