#pragma once
#include "compat/atomic.hpp"
#include "compat/defs.hpp"

namespace floormat {

// A waiter that finds its condition false calls prepare_wait(), checks the condition again,
// then calls cancel_wait() if it became true, else commit_wait() with the key. Whoever makes
// it true calls notify_*() afterwards.
class EventCount final
{
    // Two words because a futex waits on 4 bytes and the epoch needs all 32 bits.
    volatile uint32_t _epoch = 0;
    volatile uint32_t _waiters = 0;

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

    [[nodiscard]] uint32_t prepare_wait() noexcept
    {
        atomic_fetch_add(&_waiters, 1u, memory_order::relaxed);
        atomic_thread_fence(memory_order::seq_cst);
        return atomic_load(&_epoch, memory_order::acquire);
    }

    void cancel_wait() noexcept
    {
        atomic_fetch_sub(&_waiters, 1u, memory_order::relaxed);
    }

    void commit_wait(uint32_t key) noexcept
    {
        atomic_wait(&_epoch, key, memory_order::acquire);
        atomic_fetch_sub(&_waiters, 1u, memory_order::relaxed);
    }

    void notify_one() noexcept { notify(false); }
    void notify_all() noexcept { notify(true); }
};

} // namespace floormat
