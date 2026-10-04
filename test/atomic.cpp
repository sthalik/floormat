#include "app.hpp"
#include "compat/atomic.hpp"
#include <array>
#include <latch>
#include <limits>
#include <thread>

namespace floormat {

namespace {

constexpr int num_threads = 128;

template<typename F>
void run_threads(int count, F&& fn)
{
    std::latch start_gun(count);
    std::array<std::thread, num_threads> threads;
    fm_assert(count <= num_threads);

    for (int i = 0; i < count; ++i)
        threads[(uint32_t)i] = std::thread([&, i] {
            start_gun.arrive_and_wait();
            fn(i);
        });
    for (int i = 0; i < count; ++i)
        threads[(uint32_t)i].join();
}

template<typename T>
void test_semantics()
{
    using limits = std::numeric_limits<T>;
    volatile T x = 0;

    atomic_store(&x, 42);
    fm_assert(atomic_load(&x) == 42);
    atomic_store(&x, 43, memory_order::release);
    fm_assert(atomic_load(&x, memory_order::acquire) == 43);
    atomic_store(&x, 44, memory_order::relaxed);
    fm_assert(atomic_load(&x, memory_order::relaxed) == 44);

    fm_assert(atomic_exchange(&x, 5) == 44);
    fm_assert(atomic_load(&x) == 5);

    T expected = 5;
    fm_assert(atomic_compare_exchange(&x, expected, 6));
    fm_assert(expected == 5);
    fm_assert(atomic_load(&x) == 6);

    expected = 7;
    fm_assert(!atomic_compare_exchange(&x, expected, 8, memory_order::acq_rel));
    fm_assert(expected == 6);
    fm_assert(atomic_load(&x) == 6);

    expected = 6;
    while (!atomic_compare_exchange_weak(&x, expected, 9, memory_order::release))
        fm_assert(expected == 6);
    fm_assert(atomic_load(&x) == 9);

    fm_assert(atomic_fetch_add(&x, 3) == 9);
    fm_assert(atomic_fetch_sub(&x, 2) == 12);
    fm_assert(atomic_load(&x) == 10);

    atomic_store(&x, limits::max());
    fm_assert(atomic_fetch_add(&x, 1) == limits::max());
    fm_assert(atomic_load(&x) == limits::min());
    fm_assert(atomic_fetch_sub(&x, 1) == limits::min());
    fm_assert(atomic_load(&x) == limits::max());

    atomic_store(&x, 0b1100);
    fm_assert(atomic_fetch_and(&x, 0b1010) == 0b1100);
    fm_assert(atomic_load(&x) == 0b1000);
    fm_assert(atomic_fetch_or(&x, 0b0011) == 0b1000);
    fm_assert(atomic_load(&x) == 0b1011);
    fm_assert(atomic_fetch_xor(&x, 0b1111) == 0b1011);
    fm_assert(atomic_load(&x) == 0b0100);

    // non-volatile storage binds too
    T y = 1;
    fm_assert(atomic_exchange(&y, 2) == 1);
    fm_assert(atomic_load(&y) == 2);
}

void test_fence()
{
    atomic_thread_fence(memory_order::relaxed);
    atomic_thread_fence(memory_order::acquire);
    atomic_thread_fence(memory_order::release);
    atomic_thread_fence(memory_order::acq_rel);
    atomic_thread_fence(memory_order::seq_cst);
    cpu_relax();
}

constexpr int iterations = 10'000;

void test_fetch_add_contention()
{
    volatile int32_t counter = 0;
    run_threads(num_threads, [&](int) {
        for (int j = 0; j < iterations; ++j)
            atomic_fetch_add(&counter, 1, memory_order::relaxed);
    });
    fm_assert(atomic_load(&counter) == num_threads * iterations);
}

void test_cas_contention()
{
    volatile uint64_t counter = 0;
    run_threads(num_threads, [&](int) {
        for (int j = 0; j < iterations; ++j)
        {
            uint64_t expected = atomic_load(&counter, memory_order::relaxed);
            while (!atomic_compare_exchange_weak(&counter, expected, expected + 1, memory_order::relaxed))
                cpu_relax();
        }
    });
    fm_assert(atomic_load(&counter) == uint64_t{num_threads} * iterations);
}

void test_fetch_sub_contention()
{
    volatile int64_t counter = int64_t{num_threads} * iterations;
    run_threads(num_threads, [&](int) {
        for (int j = 0; j < iterations; ++j)
            atomic_fetch_sub(&counter, 1, memory_order::acq_rel);
    });
    fm_assert(atomic_load(&counter) == 0);
}

void test_bitwise_contention()
{
    // Each thread owns one bit and flips it repeatedly. A non-atomic RMW
    // loses other threads' flips, which shows up as a wrong owned bit.
    volatile uint64_t bits = 0;
    run_threads(64, [&](int i) {
        const uint64_t bit = uint64_t{1} << i;
        for (int j = 0; j < iterations; ++j)
        {
            fm_assert(!(atomic_fetch_or(&bits, bit) & bit));
            fm_assert(atomic_fetch_and(&bits, ~bit) & bit);
            fm_assert(!(atomic_fetch_xor(&bits, bit) & bit));
            fm_assert(atomic_fetch_xor(&bits, bit) & bit);
        }
        atomic_fetch_or(&bits, bit);
    });
    fm_assert(atomic_load(&bits) == ~uint64_t{0});
}

void test_release_acquire()
{
    constexpr int rounds = 10'000;
    int payload = 0;           // not atomic, published by the flag
    volatile uint8_t turn = 0; // 0: producer's turn, 1: consumer's turn

    run_threads(2, [&](int i) {
        if (i == 0)
            for (int r = 1; r <= rounds; ++r)
            {
                while (atomic_load(&turn, memory_order::acquire) != 0)
                    std::this_thread::yield();
                payload = r;
                atomic_store(&turn, 1, memory_order::release);
            }
        else
            for (int r = 1; r <= rounds; ++r)
            {
                while (atomic_load(&turn, memory_order::acquire) != 1)
                    std::this_thread::yield();
                fm_assert(payload == r);
                atomic_store(&turn, 0, memory_order::release);
            }
    });
    fm_assert(payload == rounds);
}

} // namespace

void Test::test_atomic()
{
    test_semantics<int8_t>();
    test_semantics<uint8_t>();
    test_semantics<int16_t>();
    test_semantics<uint16_t>();
    test_semantics<int32_t>();
    test_semantics<uint32_t>();
    test_semantics<int64_t>();
    test_semantics<uint64_t>();
    test_fence();

    test_fetch_add_contention();
    test_cas_contention();
    test_fetch_sub_contention();
    test_bitwise_contention();
    test_release_acquire();
}

} // namespace floormat
