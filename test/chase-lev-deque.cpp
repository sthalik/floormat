#include "app.hpp"
#include "compat/atomic.hpp"
#include "compat/chase-lev-deque.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <latch>
#include <thread>

namespace floormat {

namespace {

using namespace std::chrono_literals;
using std::chrono::steady_clock;
using Deque = ChaseLevDeque<int>;

void test_single_threaded()
{
    constexpr int count = 37;
    std::array<int, count> items;
    for (int i = 0; i < count; ++i)
        items[(uint32_t)i] = i;

    Deque q{1, 1};
    fm_assert(!q.take());
    fm_assert(!q.steal(0));

    for (int& x : items)
        q.push(&x);
    for (int i = count - 1; i >= 0; --i)
        fm_assert(q.take() == &items[(uint32_t)i]);
    fm_assert(!q.take());

    for (int& x : items)
        q.push(&x);
    for (int i = 0; i < count; ++i)
        fm_assert(q.steal(0) == &items[(uint32_t)i]);
    fm_assert(!q.steal(0));
    fm_assert(!q.take());

    for (int round = 0; round < 20; ++round)
    {
        q.push(&items[0]);
        q.push(&items[1]);
        q.push(&items[2]);
        fm_assert(q.steal(0) == &items[0]);
        fm_assert(q.take() == &items[2]);
        fm_assert(q.steal(0) == &items[1]);
        fm_assert(!q.take());
    }
}

template<uint32_t N, typename F>
void run_threads(F&& fn)
{
    std::latch start_gun(N);
    std::array<std::thread, N> threads;
    for (uint32_t i = 0; i < N; ++i)
        threads[i] = std::thread([&, i] {
            start_gun.arrive_and_wait();
            fn(i);
        });
    for (uint32_t i = 0; i < N; ++i)
        threads[i].join();
}

template<uint32_t thieves, uint32_t rounds>
void test_steal_while_growing()
{
    constexpr uint32_t count = 4096;
    std::array<int, count> items;
    volatile uint8_t seen[count];
    for (uint32_t i = 0; i < count; ++i)
        items[i] = (int)i;

    for (uint32_t r = 0; r < rounds; ++r)
    {
        for (auto& x : seen)
            x = 0;
        Deque q{thieves, 1};
        volatile uint8_t done = 0, stolen = 0;

        auto claim = [&](int* x) {
            fm_assert(x >= &items[0] && x <= &items[count - 1]);
            fm_assert(atomic_fetch_add(&seen[(uint32_t)(x - &items[0])], uint8_t{1}) == 0);
        };

        auto push_and_wait_for_steal = [&](int* x) {
            // Before the push, whose release orders it before any steal of x.
            atomic_store(&stolen, uint8_t{0}, memory_order::relaxed);
            q.push(x);
            const auto deadline = steady_clock::now() + 1s;
            while (!atomic_load(&stolen, memory_order::relaxed))
            {
                fm_assert(steady_clock::now() < deadline);
                std::this_thread::yield();
            }
        };

        run_threads<thieves + 1>([&](uint32_t i) {
            if (i == thieves)
            {
                // Pushes outpace takes, so the deque keeps growing.
                for (uint32_t k = 0; k < count; ++k)
                {
                    // Otherwise the owner can finish within one time slice, before a thief
                    // sharing its CPU runs. Powers of two spread the steals over buffer sizes.
                    if (std::has_single_bit(k + 1))
                        push_and_wait_for_steal(&items[k]);
                    else
                        q.push(&items[k]);
                    if (k % 3 == 2)
                        if (int* x = q.take())
                            claim(x);
                }
                while (int* x = q.take())
                    claim(x);
                atomic_store(&done, uint8_t{1}, memory_order::release);
            }
            else
                for (;;)
                {
                    if (int* x = q.steal(i))
                    {
                        claim(x);
                        atomic_store(&stolen, uint8_t{1}, memory_order::relaxed);
                    }
                    else if (atomic_load(&done, memory_order::acquire))
                        break;
                    else // With more thieves than cores, spinning starves the owner.
                        std::this_thread::yield();
                }
        });

        for (auto& x : seen)
            fm_assert(x == 1);
    }
}

void test_last_element()
{
    constexpr uint32_t rounds = 20'000, max_delay = 128;
    int item = 0;
    Deque q{1, 1};
    volatile uint32_t arrived = 0, thief_done = 0;
    volatile uint8_t thief_got = 0;
    uint32_t owner_wins = 0, thief_wins = 0;

    auto barrier = [&](uint32_t r) {
        atomic_fetch_add(&arrived, 1u, memory_order::acq_rel);
        while (atomic_load(&arrived, memory_order::acquire) < 2 * r)
            cpu_relax();
    };

    run_threads<2>([&](uint32_t i) {
        for (uint32_t r = 1; r <= rounds; ++r)
            if (i == 0)
            {
                q.push(&item);
                barrier(r);
                // Without a delay the thief almost never wins. A volatile loop is finer
                // grained than cpu_relax(), so the sweep crosses the length of steal().
                volatile uint32_t spin = 0;
                for (uint32_t k = r % max_delay; k; --k)
                    spin = spin + 1;
                int* x = q.take();
                while (atomic_load(&thief_done, memory_order::acquire) != r)
                    cpu_relax();
                bool thief = atomic_load(&thief_got, memory_order::relaxed);
                fm_assert((x != nullptr) != thief);
                (x ? owner_wins : thief_wins)++;
            }
            else
            {
                barrier(r);
                int* x = q.steal(0);
                fm_assert(!x || x == &item);
                atomic_store(&thief_got, uint8_t{x != nullptr}, memory_order::relaxed);
                atomic_store(&thief_done, r, memory_order::release);
            }
    });
    fm_assert(owner_wins + thief_wins == rounds);
    fm_assert(owner_wins > 0 && thief_wins > 0);
    fm_assert(!q.take());
}

} // namespace

void Test::test_chase_lev_deque()
{
    test_single_threaded();
    test_steal_while_growing<3, 40>();
    // More thieves than any CI runner has cores.
    test_steal_while_growing<128, 2>();
    test_last_element();
}

} // namespace floormat
