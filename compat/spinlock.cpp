#include "spinlock.hpp"
#include "assert.hpp"

#ifdef _MSC_VER
#include <windows.h>
#else
#if defined __i386__ || defined __x86_64__
#include <immintrin.h>
#endif
#endif

namespace floormat {

#ifdef _MSC_VER

void Spinlock::lock() noexcept
{
    while (InterlockedCompareExchange(&state, 1, 0) != 0)
        YieldProcessor();
}

bool Spinlock::try_lock() noexcept
{
    return InterlockedCompareExchange(&state, 1, 0) == 0;
}

void Spinlock::unlock() noexcept
{
    InterlockedExchange(&state, 0);
}

#else

void Spinlock::lock() noexcept
{
    for (;;)
    {
        while (__atomic_load_n(&state, __ATOMIC_RELAXED))
#if defined __x86_64__ || defined __i386__
            _mm_pause()
#elif defined __aarch64__
            asm volatile("yield")
#elif defined __riscv
            asm volatile("pause")
#elif defined __powerpc__ || defined __powerpc64__
            asm volatile("or 27,27,27")
#else
            asm volatile("" ::: "memory")
#endif
            ;

        // Phase 2: try to acquire (acquire)
        int expected = 0;
        if (__atomic_compare_exchange_n(
            &state, &expected, 1,
            false,
            __ATOMIC_ACQUIRE,
            __ATOMIC_RELAXED))
            return;
    }
}

bool Spinlock::try_lock() noexcept
{
    int expected = 0;
    return __atomic_compare_exchange_n(
        &state, &expected, 1,
        false,
        __ATOMIC_ACQUIRE,
        __ATOMIC_RELAXED);
}

void Spinlock::unlock() noexcept
{
    __atomic_store_n(&state, 0, __ATOMIC_RELEASE);
}

#endif

template<LockC T> Locker<T>::Locker(T& lock) noexcept: L{lock}
{
    L.lock();
    locked = true;
}

template<LockC T> Locker<T>::~Locker() noexcept
{
    if (locked)
        L.unlock();
}

template<LockC T> void Locker<T>::lock()
{
    fm_assert(!locked);
    L.lock();
    locked = true;
}

template<LockC T> void Locker<T>::unlock()
{
    fm_assert(locked);
    L.unlock();
    locked = false;
}

template class Locker<Spinlock>;

} // namespace floormat
