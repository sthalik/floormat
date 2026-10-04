#include "spinlock.hpp"
#include "assert.hpp"
#include "atomic.hpp"

namespace floormat {

void Spinlock::lock() noexcept
{
    for (;;)
    {
        while (atomic_load(&state, memory_order::relaxed))
            cpu_relax();

        int32_t expected = 0;
        if (atomic_compare_exchange_weak(&state, expected, 1, memory_order::acquire))
            return;
    }
}

bool Spinlock::try_lock() noexcept
{
    int32_t expected = 0;
    return atomic_compare_exchange(&state, expected, 1, memory_order::acquire);
}

void Spinlock::unlock() noexcept
{
    atomic_store(&state, 0, memory_order::release);
}

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
