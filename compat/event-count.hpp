#pragma once
#include "assert.hpp"
#include "atomic.hpp"
#include "defs.hpp"

namespace floormat {

// A waiter that finds its condition false calls prepare_wait(), checks the condition again,
// then calls cancel_wait() if it became true, else commit_wait() with the key. Whoever makes
// it true calls notify_*() afterwards.
class EventCount final
{
    // Two words because a futex waits on 4 bytes and the epoch needs all 32 bits.
    volatile u32 _epoch = 0;
    volatile u32 _waiters = 0;

    void notify(bool all) noexcept
    {
        // Pairs with the fence in prepare_wait(): either the waiter's second check sees the
        // caller's change, or this load sees the waiter.
        atomic_thread_fence(memory_order::seq_cst);
        if (!atomic_load(&_waiters, memory_order::relaxed))
            return;
        atomic_fetch_add(&_epoch, 1u, memory_order::release);
        if (all)
            atomic_notify_all(&_epoch);
        else
            atomic_notify_one(&_epoch);
    }

public:
    EventCount() noexcept = default;
    fm_DISABLE_MOVE_COPY(EventCount);

    [[nodiscard]] u32 prepare_wait() noexcept
    {
        atomic_fetch_add(&_waiters, 1u, memory_order::relaxed);
        // Pairs with the fence in notify().
        atomic_thread_fence(memory_order::seq_cst);
        // The load is acquire because a waiter that reads the epoch a notify() call stored must also see
        // the change made before that call. With a relaxed load, GenMC finds a lost wake-up under RC11.
        return atomic_load(&_epoch, memory_order::acquire);
    }

    void cancel_wait() noexcept
    {
        u32 old_waiter_count = atomic_fetch_sub(&_waiters, 1u, memory_order::relaxed);
        (void)old_waiter_count;
        fm_debug3_assert(old_waiter_count != 0);
    }

    void commit_wait(u32 key) noexcept
    {
        atomic_wait(&_epoch, key, memory_order::acquire);
        u32 old_waiter_count = atomic_fetch_sub(&_waiters, 1u, memory_order::relaxed);
        (void)old_waiter_count;
        fm_debug3_assert(old_waiter_count != 0);
    }

    // Returns the number of threads between prepare_wait() and the end of commit_wait() or cancel_wait().
    u32 num_waiters() const noexcept { return atomic_load(&_waiters, memory_order::relaxed); }

    // Every waiter must wait for the same condition. Otherwise the one woken may find its own condition
    // false and sleep again, while the waiter whose condition became true stays asleep.
    void notify_one() noexcept { notify(false); }
    void notify_all() noexcept { notify(true); }
};

} // namespace floormat
