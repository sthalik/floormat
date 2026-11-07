#include "app.hpp"
#include "compat/atomic.hpp"
#include "compat/combinations.hpp"
#include "compat/fwd/chrono.hpp"
#include "compat/fwd/thread.hpp"
#include "compat/zip.hpp"
#include <array>
#include <iterator>
#include <latch>
#include <type_traits>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <time.h>
#endif

namespace floormat {

namespace {

using namespace std::chrono_literals;
using std::chrono::steady_clock;

// Polls, since atomic_wait has no timeout and the codebase has no mutex.
class Watchdog
{
    volatile uptr test = (uptr)"", type = (uptr)"";
    volatile u8 done = 0;
    std::thread thread;

public:
    explicit Watchdog(std::chrono::seconds timeout) :
        thread{[this, timeout] {
            const auto deadline = steady_clock::now() + timeout;
            while (!atomic_load(&done, memory_order::acquire))
            {
                if (steady_clock::now() > deadline)
                    fm_abort("atomic_wait hung in %s<%s>", (const char*)atomic_load(&test), (const char*)atomic_load(&type));
                std::this_thread::sleep_for(10ms);
            }
        }}
    {}

    ~Watchdog() noexcept
    {
        atomic_store(&done, u8{1}, memory_order::release);
        thread.join();
    }

    void enter(const char* test_, const char* type_)
    {
        atomic_store(&type, (uptr)type_);
        atomic_store(&test, (uptr)test_);
    }
};

template<typename F>
void run_threads(u32 count, F&& fn)
{
    std::latch start_gun{(isize)count};
    std::array<std::thread, 40> threads;
    fm_assert(count <= threads.size());
    for (u32 i = 0; i < count; ++i)
        threads[i] = std::thread([&, i] {
            start_gun.arrive_and_wait();
            fn(i);
        });
    for (u32 i = 0; i < count; ++i)
        threads[i].join();
}

void yield() { std::this_thread::yield(); }

template<typename T> using U = std::make_unsigned_t<T>;
template<typename T> constexpr u32 bits = sizeof(T) * 8;
template<typename T> constexpr T all_bits = T(U<T>(~U<T>(0)));
template<typename T> constexpr T high_bit = T(U<T>(U<T>(1) << (bits<T> - 1)));
template<typename T> constexpr T high_half = T(U<T>(U<T>(1) << bits<T> / 2));
template<typename T> constexpr T every_byte(u8 b) { return T(U<T>(U<T>(U<T>(all_bits<T>) / 0xff) * b)); }

template<typename T>
constexpr std::array<T, 9> special_values = {
    T(0), T(1), T(2), all_bits<T>, high_bit<T>,
    T(U<T>(U<T>(all_bits<T>) >> 1)), high_half<T>,
    every_byte<T>(0x5a), every_byte<T>(0xa5),
};
static_assert(every_byte<u64>(0xa5) == 0xa5a5a5a5a5a5a5a5u);
static_assert(every_byte<u8>(0xa5) == 0xa5);

constexpr memory_order load_orders[] = { memory_order::relaxed, memory_order::acquire, memory_order::seq_cst };

template<typename T>
concept can_wait = requires (volatile T* p, T x) {
    atomic_wait(p, x);
    atomic_notify_one(p);
    atomic_notify_all(p);
};

static_assert(!can_wait<bool>);
static_assert(!can_wait<char>);
static_assert(!can_wait<wchar_t>);
static_assert(!can_wait<char8_t>);
static_assert(!can_wait<float>);
static_assert(!can_wait<double>);
static_assert(!can_wait<int*>);
static_assert(!can_wait<memory_order>);

constexpr bool sizes_valid()
{
    u32 prev = 0;
    for (auto x : atomic_wait_sizes)
    {
        if (x != 1 && x != 2 && x != 4 && x != 8)
            return false;
        if (x <= prev)
            return false;
        prev = x;
    }
    return true;
}
static_assert(std::size(atomic_wait_sizes) > 0);
static_assert(sizes_valid());
static_assert(atomic_wait_size_supported(4));
static_assert(!atomic_wait_size_supported(0));
static_assert(!atomic_wait_size_supported(3));
static_assert(!atomic_wait_size_supported(16));
#ifdef _WIN32
static_assert(std::size(atomic_wait_sizes) == 4);
#endif

u64 thread_cpu_time()
{
#ifdef _WIN32
    ULONG64 cycles = 0;
    QueryThreadCycleTime(GetCurrentThread(), &cycles);
    return cycles;
#else
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (u64)ts.tv_sec * 1'000'000'000u + (u64)ts.tv_nsec;
#endif
}

// thread_cpu_time() units per second of busy work. Cycles on Windows, nanoseconds elsewhere.
// Keeps the fastest of several runs, since a run preempted midway reads low and tightens the bound in test_sleeps().
double cpu_time_rate()
{
    double rate = 0;
    for (u32 i = 0; i < 3; ++i)
    {
        auto t0 = steady_clock::now();
        auto c0 = thread_cpu_time();
        while (steady_clock::now() - t0 < 10ms)
            cpu_relax();
        auto c1 = thread_cpu_time();
        double r = (double)(c1 - c0) / std::chrono::duration<double>(steady_clock::now() - t0).count();
        if (r > rate)
            rate = r;
    }
    return rate;
}

template<typename T>
void test_values_distinct()
{
    constexpr auto& v = special_values<T>;
    for (const auto& [a, b] : combinations(v))
        fm_assert(a != b);
}

template<typename T>
void test_returns_at_once()
{
    for (T cur : special_values<T>)
        for (T old : special_values<T>)
            if (cur != old)
                for (auto o : load_orders)
                {
                    volatile T x = cur;
                    atomic_wait(&x, old, o);
                    fm_assert(atomic_load(&x) == cur);
                    T y = cur;
                    atomic_wait(&y, old, o);
                }
}

template<typename T>
void test_notify_without_waiters()
{
    volatile T stack[64] = {};
    for (auto& x : stack)
    {
        atomic_notify_one(&x);
        atomic_notify_all(&x);
    }
    T* heap = new T{};
    atomic_notify_one(heap);
    atomic_notify_all(heap);
    delete heap;
}

// A waiter that never reaches the kernel sleep, or whose OS compare never matches, spins in
// atomic_wait's loop and burns CPU time for as long as the wake is delayed.
// Returns the largest share of a busy thread's CPU time that any waiter used.
template<typename T>
double sleeps_cpu_share(double rate)
{
    struct pair { T old, next; };
    constexpr pair pairs[] = {
        { T(0), high_half<T> },
        { all_bits<T>, T(U<T>(U<T>(all_bits<T>) >> 1)) },
        { high_bit<T>, T(0) },
        { high_half<T>, T(high_half<T> | 1) },
    };
    constexpr u32 n = std::size(pairs);
    struct alignas(64) slot
    {
        volatile T value;
        volatile u8 ready = 0;
        u64 cpu = 0;
        double wall = 0;
    };
    slot slots[n];
    for (auto& [s, p] : zip(slots, pairs))
        s.value = p.old;

    run_threads(n + 1, [&](u32 i) {
        if (i < n)
        {
            auto& s = slots[i];
            // Read before `ready`, so the waker's whole sleep falls inside the measured interval.
            auto t0 = steady_clock::now();
            auto c0 = thread_cpu_time();
            atomic_store(&s.ready, u8{1});
            atomic_wait(&s.value, pairs[i].old);
            s.cpu = thread_cpu_time() - c0;
            s.wall = std::chrono::duration<double>(steady_clock::now() - t0).count();
            fm_assert(atomic_load(&s.value) == pairs[i].next);
        }
        else
        {
            for (auto& s : slots)
                while (!atomic_load(&s.ready))
                    yield();
            std::this_thread::sleep_for(30ms);
            for (u32 k = 0; k < n; ++k)
            {
                atomic_store(&slots[k].value, pairs[k].next);
                atomic_notify_one(&slots[k].value);
            }
        }
    });

    double share = 0;
    for (const auto& s : slots)
    {
        fm_assert(s.wall >= 0.025);
        double x = (double)s.cpu / (rate * s.wall);
        if (x > share)
            share = x;
    }
    return share;
}

// A host that preempts a VM's vCPU bills the stolen time to the guest thread running on it, so
// one attempt over the bound is noise. A waiter that spins goes over on every attempt.
template<typename T>
void test_sleeps(double rate)
{
    double share = 0;
    for (u32 i = 0; i < 3; ++i)
        if ((share = sleeps_cpu_share<T>(rate)) < 0.25)
            return;
    fm_abort("atomic_wait waiter used %.0f%% of a CPU while asleep", share * 100);
}

template<typename T>
void test_ping_pong(T a, T b, bool notify_all)
{
    constexpr u32 rounds = 500;
    const T values[2] = { a, b };
    volatile T turn = a;
    u32 payload = 0; // not atomic, published by `turn`

    run_threads(2, [&](u32 i) {
        const T theirs = values[1 - i];
        for (u32 r = 0; r < rounds; ++r)
        {
            atomic_wait(&turn, theirs, memory_order::acquire);
            fm_assert(atomic_load(&turn, memory_order::relaxed) == values[i]);
            fm_assert(payload == 2 * r + i);
            payload++;
            atomic_store(&turn, theirs, memory_order::release);
            if (notify_all)
                atomic_notify_all(&turn);
            else
                atomic_notify_one(&turn);
        }
    });
    fm_assert(payload == 2 * rounds);
}

template<typename T>
void test_ring()
{
    constexpr u32 threads = 8, laps = 100;
    auto turn_of = [](u32 i) { return T(U<T>(U<T>(i) << (bits<T> - 3))); };
    volatile T turn = turn_of(0);
    u32 payload = 0; // not atomic, published by `turn`

    run_threads(threads, [&](u32 i) {
        for (u32 lap = 0; lap < laps; ++lap)
        {
            for (;;)
            {
                T v = atomic_load(&turn, memory_order::acquire);
                if (v == turn_of(i))
                    break;
                atomic_wait(&turn, v, memory_order::relaxed);
            }
            fm_assert(payload == lap * threads + i);
            payload++;
            atomic_store(&turn, turn_of((i + 1) % threads), memory_order::release);
            atomic_notify_all(&turn);
        }
    });
    fm_assert(payload == threads * laps);
}

// Neighbouring 1- and 2-byte values share a word, and every waiter here may share a pool
// bucket. A wake meant for one address must not let a waiter on another return.
template<typename T>
void test_many_addresses()
{
    constexpr u32 n = 32, stride = 13;
    static_assert(n % stride != 0 && stride % 2 == 1);
    volatile T values[n] = {};
    volatile u8 arrived[n] = {}, returned[n] = {};

    run_threads(n + 1, [&](u32 i) {
        if (i < n)
        {
            atomic_store(&arrived[i], u8{1});
            atomic_wait(&values[i], T(0));
            atomic_store(&returned[i], u8{1});
        }
        else
        {
            for (auto& x : arrived)
                while (!atomic_load(&x))
                    yield();
            std::this_thread::sleep_for(5ms);
            for (u32 j = 0; j < n; ++j)
            {
                u32 k = j * stride % n;
                atomic_store(&values[k], T(1));
                atomic_notify_one(&values[k]);
                while (!atomic_load(&returned[k]))
                    yield();
                for (auto& [v, r] : zip(values, returned))
                    if (atomic_load(&v) == T(0))
                        fm_assert(!atomic_load(&r));
            }
        }
    });
}

template<typename T>
void test_notify_unchanged()
{
    constexpr T old = all_bits<T>;
    volatile T value = old;
    volatile u8 arrived = 0, returned = 0;

    run_threads(2, [&](u32 i) {
        if (i == 0)
        {
            atomic_store(&arrived, u8{1});
            atomic_wait(&value, old);
            atomic_store(&returned, u8{1});
        }
        else
        {
            while (!atomic_load(&arrived))
                yield();
            for (u32 k = 0; k < 200; ++k)
            {
                if (k % 2)
                    atomic_notify_all(&value);
                else
                    atomic_notify_one(&value);
                yield();
                fm_assert(!atomic_load(&returned));
            }
            atomic_store(&value, T(0));
            atomic_notify_one(&value);
        }
    });
    fm_assert(atomic_load(&returned));
}

template<typename T>
void test_wake_sleepers(bool notify_all)
{
    constexpr u32 waiters = 8;
    volatile T value = high_bit<T>;
    volatile u32 arrived = 0, returned = 0;

    run_threads(waiters + 1, [&](u32 i) {
        if (i < waiters)
        {
            atomic_fetch_add(&arrived, 1u);
            atomic_wait(&value, high_bit<T>);
            atomic_fetch_add(&returned, 1u);
        }
        else
        {
            while (atomic_load(&arrived) != waiters)
                yield();
            // Lets the waiters reach the kernel. One still awake sees the new value and
            // returns, which can only hide a bug.
            std::this_thread::sleep_for(5ms);
            atomic_store(&value, T(0));
            if (notify_all)
                atomic_notify_all(&value);
            else
                for (u32 k = 0; k < waiters; ++k)
                    atomic_notify_one(&value);
        }
    });
    fm_assert(atomic_load(&returned) == waiters);
}

template<typename T>
void test_counter()
{
    constexpr u32 producers = 4, consumers = 3, per_producer = 30;
    constexpr T target = T(producers * per_producer);
    static_assert(producers * per_producer <= 127);
    volatile T counter = 0;

    run_threads(producers + consumers, [&](u32 i) {
        if (i < producers)
            for (u32 k = 0; k < per_producer; ++k)
            {
                atomic_fetch_add(&counter, T(1), memory_order::release);
                atomic_notify_all(&counter);
                if (k % 4 == 0)
                    yield();
            }
        else
            for (;;)
            {
                T v = atomic_load(&counter, memory_order::acquire);
                if (v == target)
                    break;
                atomic_wait(&counter, v, memory_order::acquire);
            }
    });
}

template<typename T>
void test_type(Watchdog& w, const char* name, double rate)
{
    static_assert(can_wait<T> == atomic_wait_size_supported(sizeof(T)));
    static_assert(can_wait<T> == atomic_wait_type<T>);

    if constexpr (atomic_wait_type<T>)
    {
        test_values_distinct<T>();
        w.enter("returns_at_once", name);
        test_returns_at_once<T>();
        w.enter("notify_without_waiters", name);
        test_notify_without_waiters<T>();
        w.enter("sleeps", name);
        test_sleeps<T>(rate);
        w.enter("ping_pong", name);
        test_ping_pong<T>(T(0), high_bit<T>, false);
        test_ping_pong<T>(all_bits<T>, T(1), true);
        w.enter("ring", name);
        test_ring<T>();
        w.enter("many_addresses", name);
        test_many_addresses<T>();
        w.enter("notify_unchanged", name);
        test_notify_unchanged<T>();
        w.enter("wake_sleepers", name);
        test_wake_sleepers<T>(false);
        test_wake_sleepers<T>(true);
        w.enter("counter", name);
        test_counter<T>();
    }
}

} // namespace

void Test::test_atomic_wait()
{
    Watchdog w{120s};
    const double rate = cpu_time_rate();
    fm_assert(rate > 0);

    test_type<signed char>(w, "signed char", rate);
    test_type<unsigned char>(w, "unsigned char", rate);
    test_type<short>(w, "short", rate);
    test_type<unsigned short>(w, "unsigned short", rate);
    test_type<int>(w, "int", rate);
    test_type<unsigned>(w, "unsigned", rate);
    test_type<long>(w, "long", rate);
    test_type<unsigned long>(w, "unsigned long", rate);
    test_type<long long>(w, "long long", rate);
    test_type<unsigned long long>(w, "unsigned long long", rate);
}

} // namespace floormat
