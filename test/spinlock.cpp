#include "app.hpp"
#include "compat/spinlock.hpp"
//#include <cr/Debug.h>
#include <array>
#include <latch>
#include <thread>

namespace floormat {

namespace {

void test1()
{
    {
        Spinlock sp;
        Locker l{sp};
    }
}

void test2()
{
    {
        Spinlock sp;
        Locker l{sp};
        sp.unlock();
        sp.lock();
        fm_assert(!sp.try_lock());
    }
    {
        Spinlock sp;
        Locker l{sp};
        sp.unlock();
        sp.lock();
        sp.unlock();
        fm_assert(sp.try_lock());
        sp.unlock();
    }
}

void test_spinlock_contention() // by ai
{
    constexpr int num_threads = 8;
    constexpr int iterations = 1'000'000;
    int shared_counter = 0;

    Spinlock spin;
    std::latch start_gun(num_threads);
    std::array<std::thread, num_threads> threads;

    for (auto i = 0u; i < num_threads; ++i)
    {
        threads[i] = std::thread([&]
        {
            start_gun.arrive_and_wait(); // Wait for all threads to spawn

            for (int j = 0; j < iterations; ++j)
            {
                spin.lock();
                shared_counter++; // Critical section
                spin.unlock();
            }
        });
    }

    for (auto& t : threads)
        t.join();
    fm_assert(shared_counter == num_threads * iterations);
}

} // namespace

void Test::test_spinlock()
{
    test1();
    test2();
    test_spinlock_contention();
}

} // namespace floormat
