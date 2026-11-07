#include "app.hpp"
#include "compat/atomic.hpp"
#include "compat/enumerate.hpp"
#include "compat/event-count.hpp"
#include "compat/fwd/chrono.hpp"
#include "compat/fwd/thread.hpp"
#include <array>
#include <latch>

namespace floormat {

namespace {

using namespace std::chrono_literals;
using std::chrono::steady_clock;

template<u32 N, typename F>
void run_threads(F&& fn)
{
    std::latch start_gun(N);
    std::array<std::thread, N> threads;
    for (auto& [i, t] : enumerate(threads))
        t = std::thread([&, i] {
            start_gun.arrive_and_wait();
            fn(i);
        });
    for (u32 i = 0; i < N; ++i)
        threads[i].join();
}

template<typename F>
void poll_until(F&& done)
{
    const auto deadline = steady_clock::now() + 10s;
    while (!done())
    {
        fm_assert(steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

void test_single_threaded()
{
    volatile u8 done = 0;
    // On its own thread, so that a commit_wait() that never returns fails the deadline.
    std::thread t{[&] {
        EventCount ec;
        ec.notify_one();
        ec.notify_all();

        u32 key = ec.prepare_wait();
        ec.cancel_wait();

        key = ec.prepare_wait();
        ec.notify_one();
        ec.commit_wait(key);

        key = ec.prepare_wait();
        ec.notify_all();
        ec.commit_wait(key);
        atomic_store(&done, u8{1}, memory_order::relaxed);
    }};
    poll_until([&] { return atomic_load(&done, memory_order::relaxed) != 0; });
    t.join();
}

// The consumer sleeps and the producer polls with a deadline, so a lost wake-up fails the
// producer's assert instead of hanging both.
template<u32 rounds>
void test_ping_pong()
{
    EventCount ec;
    volatile u32 posted = 0, taken = 0;

    run_threads<2>([&](u32 i) {
        if (i == 0)
            for (u32 k = 1; k <= rounds; ++k)
            {
                atomic_store(&posted, k, memory_order::relaxed);
                ec.notify_one();
                poll_until([&] { return atomic_load(&taken, memory_order::relaxed) == k; });
            }
        else
            for (u32 k = 1; k <= rounds; ++k)
            {
                while (atomic_load(&posted, memory_order::relaxed) != k)
                {
                    u32 key = ec.prepare_wait();
                    if (atomic_load(&posted, memory_order::relaxed) == k)
                        ec.cancel_wait();
                    else
                        ec.commit_wait(key);
                }
                atomic_store(&taken, k, memory_order::relaxed);
            }
    });
}

template<u32 consumers, u32 jobs>
void test_jobs()
{
    EventCount ec;
    volatile u32 available = 0, done = 0, exited = 0;
    volatile u8 stop = 0;

    auto claim = [&] {
        u32 n = atomic_load(&available, memory_order::relaxed);
        while (n)
            if (atomic_compare_exchange_weak(&available, n, n - 1, memory_order::relaxed))
                return true;
        return false;
    };

    run_threads<consumers + 1>([&](u32 i) {
        if (i == consumers)
        {
            for (u32 k = 0; k < jobs; ++k)
            {
                atomic_fetch_add(&available, 1u, memory_order::relaxed);
                ec.notify_one();
            }
            poll_until([&] { return atomic_load(&done, memory_order::relaxed) == jobs; });
            atomic_store(&stop, u8{1}, memory_order::relaxed);
            ec.notify_all();
            poll_until([&] { return atomic_load(&exited, memory_order::relaxed) == consumers; });
        }
        else
        {
            for (;;)
            {
                if (claim())
                    atomic_fetch_add(&done, 1u, memory_order::relaxed);
                else if (atomic_load(&stop, memory_order::relaxed))
                    break;
                else
                {
                    u32 key = ec.prepare_wait();
                    if (atomic_load(&available, memory_order::relaxed) || atomic_load(&stop, memory_order::relaxed))
                        ec.cancel_wait();
                    else
                        ec.commit_wait(key);
                }
            }
            atomic_fetch_add(&exited, 1u, memory_order::relaxed);
        }
    });
}

} // namespace

void Test::test_event_count()
{
    test_single_threaded();
    test_ping_pong<20000>();
    test_jobs<4, 20000>();
    test_jobs<32, 2000>();
}

} // namespace floormat
