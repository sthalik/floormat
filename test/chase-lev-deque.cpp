#include "app.hpp"
#include "compat/atomic.hpp"
#include "compat/chase-lev-deque.inl"
#include "compat/enumerate.hpp"
#include "compat/fwd/chrono.hpp"
#include "compat/fwd/thread.hpp"
#include "compat/valgrind.hpp"
#include <array>
#include <bit>
#include <latch>

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
        items[(u32)i] = i;

    ChaseLevBufferPool pool;
    Deque q{pool, 1};
    fm_assert(!q.take());
    fm_assert(!q.steal());

    for (int& x : items)
        q.push(&x);
    for (int i = count - 1; i >= 0; --i)
        fm_assert(q.take() == &items[(u32)i]);
    fm_assert(!q.take());

    for (int& x : items)
        q.push(&x);
    for (int i = 0; i < count; ++i)
        fm_assert(q.steal() == &items[(u32)i]);
    fm_assert(!q.steal());
    fm_assert(!q.take());

    for (int round = 0; round < 20; ++round)
    {
        q.push(&items[0]);
        q.push(&items[1]);
        q.push(&items[2]);
        fm_assert(q.steal() == &items[0]);
        fm_assert(q.take() == &items[2]);
        fm_assert(q.steal() == &items[1]);
        fm_assert(!q.take());
    }
}

void test_buffer_pool()
{
    ChaseLevBufferPool pool;
    auto* a = pool.get(3);
    auto* b = pool.get(3);
    auto* c = pool.get(4);
    fm_assert(a != b);
    pool.put(a);
    pool.put(b);
    pool.put(c);
    auto* x = pool.get(3);
    auto* y = pool.get(3);
    fm_assert((x == a && y == b) || (x == b && y == a));
    fm_assert(pool.get(4) == c);
    pool.put(x);
    pool.put(y);
    pool.put(c);
}

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

template<u32 thieves, u32 rounds>
void test_steal_while_growing()
{
    constexpr u32 count = 4096, deques = 2;
    struct state
    {
        std::array<int, count> items;
        volatile u8 seen[count];
        volatile u8 done, stolen;
    };
    std::array<state, deques> states;
    // Shared, so each deque reuses arrays the other just retired while its thieves may still
    // read them.
    ChaseLevBufferPool pool;

    for (u32 r = 0; r < rounds; ++r)
    {
        for (auto& s : states)
        {
            for (auto& x : s.seen)
                x = 0;
            s.done = 0;
            s.stolen = 0;
        }
        Deque q0{pool, 1}, q1{pool, 1};
        Deque* const qs[deques]{&q0, &q1};

        run_threads<deques * (thieves + 1)>([&](u32 i) {
            auto& s = states[i / (thieves + 1)];
            auto& q = *qs[i / (thieves + 1)];

            auto claim = [&](int* x) {
                fm_assert(x >= &s.items[0] && x <= &s.items[count - 1]);
                fm_assert(atomic_fetch_add(&s.seen[(u32)(x - &s.items[0])], u8{1}) == 0);
            };

            auto push_and_wait_for_steal = [&](int* x) {
                // Before the push, whose release orders it before any steal of x.
                atomic_store(&s.stolen, u8{0}, memory_order::relaxed);
                q.push(x);
                const auto deadline = steady_clock::now() + 1s;
                while (!atomic_load(&s.stolen, memory_order::relaxed))
                {
                    fm_assert(steady_clock::now() < deadline);
                    std::this_thread::yield();
                }
            };

            if (i % (thieves + 1) == thieves)
            {
                // Pushes outpace takes, so the deque keeps growing.
                for (auto& [k, item] : enumerate(s.items))
                {
                    // Otherwise the owner can finish within one time slice, before a thief
                    // sharing its CPU runs. Powers of two spread the steals over buffer sizes.
                    if (std::has_single_bit(k + 1))
                        push_and_wait_for_steal(&item);
                    else
                        q.push(&item);
                    if (k % 3 == 2)
                        if (int* x = q.take())
                            claim(x);
                }
                while (int* x = q.take())
                    claim(x);
                atomic_store(&s.done, u8{1}, memory_order::release);
            }
            else
                for (;;)
                {
                    if (int* x = q.steal())
                    {
                        claim(x);
                        atomic_store(&s.stolen, u8{1}, memory_order::relaxed);
                    }
                    else if (atomic_load(&s.done, memory_order::acquire))
                        break;
                    else // With more thieves than cores, spinning starves the owner.
                        std::this_thread::yield();
                }
        });

        for (auto& s : states)
            for (auto& x : s.seen)
                fm_assert(x == 1);
    }
}

struct pause_steal
{
    static inline volatile u8 paused = 0, resume = 0;

    static void after_array_load() noexcept
    {
        atomic_store(&paused, u8{1}, memory_order::release);
        while (!atomic_load(&resume, memory_order::acquire))
            std::this_thread::yield();
    }
};

// Without the _array check in steal(), the thief returns &other.
void test_reuse_during_steal()
{
    // --repeat runs this again with the flags still set.
    pause_steal::paused = 0;
    pause_steal::resume = 0;
    ChaseLevBufferPool pool;
    int items[3]{}, other = 0;
    ChaseLevDeque<int, pause_steal> q{pool, 1};
    q.push(&items[0]);
    int* x = &items[0];

    run_threads<2>([&](u32 i) {
        if (i == 0)
        {
            while (!atomic_load(&pause_steal::paused, memory_order::acquire))
                std::this_thread::yield();
            // The second push grows the array, and c's constructor takes the old one.
            q.push(&items[1]);
            q.push(&items[2]);
            Deque c{pool, 1};
            c.push(&other);
            atomic_store(&pause_steal::resume, u8{1}, memory_order::release);
            fm_assert(c.take() == &other);
        }
        else
            x = q.steal();
    });

    fm_assert(!x);
    fm_assert(q.take() == &items[2]);
    fm_assert(q.take() == &items[1]);
    fm_assert(q.take() == &items[0]);
    fm_assert(!q.take());
}

void test_last_element()
{
    constexpr u32 rounds = 20'000, max_delay = 128;
    int item = 0;
    ChaseLevBufferPool pool;
    Deque q{pool, 1};
    volatile u32 arrived = 0, thief_done = 0;
    volatile u8 thief_got = 0;
    u32 owner_wins = 0, thief_wins = 0;

    auto barrier = [&](u32 r) {
        atomic_fetch_add(&arrived, 1u, memory_order::acq_rel);
        while (atomic_load(&arrived, memory_order::acquire) < 2 * r)
            cpu_relax();
    };

    run_threads<2>([&](u32 i) {
        for (u32 r = 1; r <= rounds; ++r)
            if (i == 0)
            {
                q.push(&item);
                barrier(r);
                // Without a delay the thief almost never wins. A volatile loop is finer
                // grained than cpu_relax(), so the sweep crosses the length of steal().
                volatile u32 spin = 0;
                for (u32 k = r % max_delay; k; --k)
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
                int* x = q.steal();
                fm_assert(!x || x == &item);
                atomic_store(&thief_got, u8{x != nullptr}, memory_order::relaxed);
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
    test_buffer_pool();
    test_steal_while_growing<3, 40>();
    // More thieves than any CI runner has cores.
    test_steal_while_growing<128, 2>();
    test_reuse_during_steal();
    // Valgrind runs one thread at a time and yields on PAUSE, so the owner always wins.
    if (!is_valgrind())
        test_last_element();
}

} // namespace floormat
