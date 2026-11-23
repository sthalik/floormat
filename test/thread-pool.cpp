#include "app.hpp"
#include "compat/atomic.hpp"
#include "compat/stl/chrono.hpp"
#include "compat/stl/thread.hpp"
#include "compat/inplace-function.hpp"
#include "compat/thread-pool.hpp"
#include <array>

namespace floormat {

namespace {

using namespace std::chrono_literals;
using std::chrono::steady_clock;

struct xorshift32 final
{
    u32 state;

    u32 operator()() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
};

// Runs fn(progress) on a thread of its own and aborts when progress stays unchanged for 10 s, so that a
// hang fails the test.
template<typename F>
void run_with_progress_deadline(const char* name, F&& fn)
{
    volatile u32 progress = 0;
    volatile u8 done = 0;
    std::thread t{[&] {
        fn(progress);
        atomic_store(&done, u8{1}, memory_order::relaxed);
    }};
    u32 last_progress = 0;
    auto deadline = steady_clock::now() + 10s;
    while (!atomic_load(&done, memory_order::relaxed))
    {
        u32 p = atomic_load(&progress, memory_order::relaxed);
        if (p != last_progress)
        {
            last_progress = p;
            deadline = steady_clock::now() + 10s;
        }
        else if (steady_clock::now() >= deadline)
            fm_abort("%s: no progress for 10 s at %u", name, (unsigned)p);
        std::this_thread::yield();
    }
    t.join();
}

constexpr u32 max_count = 300, max_grain_size = 64;

void check_parallel_for_calls(ThreadPool& pool, xorshift32& rng, u32 num_calls, volatile u32& progress)
{
    const u32 num_threads = pool.num_threads();
    u32 seen[max_count];
    for (u32 k = 0; k < num_calls; ++k)
    {
        const u32 count = rng() % (max_count + 1), grain_size = rng() % max_grain_size + 1;
        for (u32 i = 0; i < count; ++i)
            seen[i] = 0;
        pool.parallel_for(count, grain_size, [&](u32 begin, u32 end, u32 worker_index) {
            fm_assert(worker_index < num_threads);
            fm_assert(pool.is_running());
            fm_assert(begin % grain_size == 0 && begin < end);
            fm_assert(end == (count - begin > grain_size ? begin + grain_size : count));
            for (u32 i = begin; i < end; ++i)
                seen[i]++;
        });
        fm_assert(!pool.is_running());
        for (u32 i = 0; i < count; ++i)
            fm_assert(seen[i] == 1);
        atomic_fetch_add(&progress, 1u, memory_order::relaxed);
    }
}

void test_coverage(u32 num_threads, u32 num_calls, u32 idle_spin_usec)
{
    run_with_progress_deadline("coverage", [&](volatile u32& progress) {
        ThreadPool pool{{.num_threads = num_threads, .idle_spin_usec = idle_spin_usec}};
        xorshift32 rng{2463534242u + num_threads};
        check_parallel_for_calls(pool, rng, num_calls, progress);
    });
}

// A caller that runs jobs would hide a lost wake-up of a worker, so this one only waits.
void test_lost_wake_up(u32 num_threads, u32 idle_spin_usec)
{
    run_with_progress_deadline("lost_wake_up", [&](volatile u32& progress) {
        constexpr u32 max_num_jobs = 8;
        ThreadPool pool{{.num_threads = num_threads, .is_caller_running_jobs = false, .idle_spin_usec = idle_spin_usec}};
        xorshift32 rng{1597334677u + num_threads};
        u32 seen[max_num_jobs];
        for (u32 k = 1; k <= 2000; ++k)
        {
            for (u32 i = rng() % 4; i != 0; --i)
                std::this_thread::yield();
            // Long enough for every worker to fall asleep.
            if (k % 16 == 0)
                std::this_thread::sleep_for(1ms);
            const u32 count = rng() % max_num_jobs + 1;
            for (u32 i = 0; i < count; ++i)
                seen[i] = 0;
            pool.parallel_for(count, 1, [&](u32 begin, u32 end, u32 worker_index) {
                fm_assert(worker_index != 0 && worker_index < num_threads);
                fm_assert(end == begin + 1);
                seen[begin]++;
            });
            for (u32 i = 0; i < count; ++i)
                fm_assert(seen[i] == 1);
            atomic_fetch_add(&progress, 1u, memory_order::relaxed);
        }
    });
}

void test_submit(u32 idle_spin_usec)
{
    constexpr u32 num_threads = 4, num_submitters = 4, num_jobs_per_submitter = 5000;
    constexpr u32 num_jobs_per_round = num_submitters * num_jobs_per_submitter;
    volatile u32 num_jobs_run = 0;

    run_with_progress_deadline("submit", [&](volatile u32& progress) {
        ThreadPool pool{{.num_threads = num_threads, .idle_spin_usec = idle_spin_usec}};
        std::array<std::thread, num_submitters> submitters;

        auto start_submitters = [&] {
            // Lets every worker fall asleep. One still awake would run every queued job and hide a lost wake-up.
            std::this_thread::sleep_for(1ms);
            for (auto& t : submitters)
                t = std::thread{[&] {
                    for (u32 k = 0; k < num_jobs_per_submitter; ++k)
                        pool.submit([&](u32 worker_index) noexcept {
                            fm_assert(worker_index != 0 && worker_index < num_threads);
                            atomic_fetch_add(&num_jobs_run, 1u, memory_order::relaxed);
                            atomic_fetch_add(&progress, 1u, memory_order::relaxed);
                        });
                }};
        };
        auto join_submitters = [&] {
            for (auto& t : submitters)
                t.join();
        };
        auto wait_for_jobs = [&](u32 n) {
            while (atomic_load(&num_jobs_run, memory_order::relaxed) < n)
                std::this_thread::yield();
        };

        start_submitters();
        join_submitters();
        wait_for_jobs(num_jobs_per_round);

        start_submitters();
        xorshift32 rng{3141592653u};
        check_parallel_for_calls(pool, rng, 200, progress);
        join_submitters();
        wait_for_jobs(2 * num_jobs_per_round);
    });
    fm_assert(atomic_load(&num_jobs_run, memory_order::relaxed) == 2 * num_jobs_per_round);
}

void test_construct_and_destroy(u32 idle_spin_usec)
{
    run_with_progress_deadline("construct_and_destroy", [&](volatile u32& progress) {
        xorshift32 rng{2718281829u};
        for (u32 k = 0; k < 1000; ++k)
        {
            const u32 num_threads = k % 5 + 1;
            volatile u32 num_jobs_run = 0;
            u32 num_jobs_submitted = 0;
            {
                ThreadPool pool{{.num_threads = num_threads, .idle_spin_usec = idle_spin_usec}};
                if (rng() % 2)
                    check_parallel_for_calls(pool, rng, 1, progress);
                if (num_threads >= 2)
                {
                    num_jobs_submitted = rng() % 3 + 1;
                    for (u32 i = 0; i < num_jobs_submitted; ++i)
                        pool.submit([&](u32) noexcept {
                            atomic_fetch_add(&num_jobs_run, 1u, memory_order::relaxed);
                        });
                }
            }
            fm_assert(atomic_load(&num_jobs_run, memory_order::relaxed) == num_jobs_submitted);
            atomic_fetch_add(&progress, 1u, memory_order::relaxed);
        }
    });
}

void test_long_job(u32 idle_spin_usec)
{
    run_with_progress_deadline("long_job", [&](volatile u32& progress) {
        constexpr u32 count = 64;
        ThreadPool pool{{.num_threads = 4, .idle_spin_usec = idle_spin_usec}};
        u32 seen[count] = {};
        pool.parallel_for(count, 1, [&](u32 begin, u32 end, u32) {
            fm_assert(end == begin + 1);
            if (begin == 0)
                std::this_thread::sleep_for(20ms);
            seen[begin]++;
        });
        for (u32 x : seen)
            fm_assert(x == 1);
        atomic_fetch_add(&progress, 1u, memory_order::relaxed);
    });
}

void test_start_handler(u32 num_threads, u32 idle_spin_usec)
{
    constexpr u32 max_num_threads = 8;
    fm_assert(num_threads <= max_num_threads);
    volatile u8 is_started[max_num_threads] = {};
    volatile u32 num_start_handler_calls = 0;

    run_with_progress_deadline("start_handler", [&](volatile u32& progress) {
        {
            ThreadPool pool{{
                .num_threads = num_threads,
                .start_handler = [&](u32 worker_index) noexcept {
                    fm_assert(worker_index != 0 && worker_index < num_threads);
                    fm_assert(!atomic_load(&is_started[worker_index], memory_order::relaxed));
                    atomic_store(&is_started[worker_index], u8{1}, memory_order::relaxed);
                    atomic_fetch_add(&num_start_handler_calls, 1u, memory_order::relaxed);
                },
                .idle_spin_usec = idle_spin_usec,
            }};
            xorshift32 rng{1103515245u + num_threads};
            for (u32 k = 0; k < 200; ++k)
            {
                pool.parallel_for(rng() % 64 + 1, 1, [&](u32, u32, u32 worker_index) {
                    fm_assert(worker_index == 0 || atomic_load(&is_started[worker_index], memory_order::relaxed));
                });
                atomic_fetch_add(&progress, 1u, memory_order::relaxed);
            }
        }
        fm_assert(atomic_load(&num_start_handler_calls, memory_order::relaxed) == num_threads - 1);
    });
}

void test_single_range(u32 idle_spin_usec)
{
    run_with_progress_deadline("single_range", [&](volatile u32& progress) {
        const auto caller_thread_id = std::this_thread::get_id();
        for (bool is_caller_running_jobs : {true, false})
        {
            ThreadPool pool{{.num_threads = 3, .is_caller_running_jobs = is_caller_running_jobs,
                             .idle_spin_usec = idle_spin_usec}};
            for (u32 count : {1u, 5u, 64u})
            {
                u32 num_body_calls = 0;
                pool.parallel_for(count, 64, [&](u32 begin, u32 end, u32 worker_index) {
                    fm_assert(begin == 0 && end == count);
                    fm_assert(pool.is_running());
                    if (is_caller_running_jobs)
                        fm_assert(worker_index == 0 && std::this_thread::get_id() == caller_thread_id);
                    else
                        fm_assert(worker_index != 0 && std::this_thread::get_id() != caller_thread_id);
                    ++num_body_calls;
                });
                fm_assert(num_body_calls == 1);
                fm_assert(!pool.is_running());
                atomic_fetch_add(&progress, 1u, memory_order::relaxed);
            }
        }
    });
}

// The calls come faster than the worker's 2 ms spin, so it finds most jobs while spinning.
void test_back_to_back_calls_while_spinning()
{
    run_with_progress_deadline("back_to_back_calls_while_spinning", [&](volatile u32& progress) {
        constexpr u32 num_calls = 2000;
        u32 num_jobs_run = 0;
        ThreadPool pool{{.num_threads = 2, .is_caller_running_jobs = false, .idle_spin_usec = 2000}};
        for (u32 k = 0; k < num_calls; ++k)
        {
            pool.parallel_for(1, 1, [&](u32 begin, u32 end, u32 worker_index) {
                fm_assert(begin == 0 && end == 1 && worker_index == 1);
                ++num_jobs_run;
            });
            atomic_fetch_add(&progress, 1u, memory_order::relaxed);
        }
        fm_assert(num_jobs_run == num_calls);
    });
}

} // namespace

void Test::test_thread_pool()
{
    for (u32 idle_spin_usec : {0u, 50u})
    {
        test_coverage(1, 2000, idle_spin_usec);
        test_coverage(2, 2000, idle_spin_usec);
        test_coverage(3, 20000, idle_spin_usec);
        test_coverage(8, 2000, idle_spin_usec);
        test_lost_wake_up(2, idle_spin_usec);
        test_lost_wake_up(5, idle_spin_usec);
        test_submit(idle_spin_usec);
        test_construct_and_destroy(idle_spin_usec);
        test_long_job(idle_spin_usec);
        test_start_handler(1, idle_spin_usec);
        test_start_handler(2, idle_spin_usec);
        test_start_handler(5, idle_spin_usec);
        test_single_range(idle_spin_usec);
    }
    test_back_to_back_calls_while_spinning();
}

} // namespace floormat
