#pragma once
#include "borrowed-ptr-policy.hpp"
#include "defs.hpp"
#include <atomic>

namespace floormat::bptr_policy {

template<typename W>
struct atomic_counter
{
    using value_type = W;
    template<typename X> using cell = std::atomic<X>;
    struct block_state {};
    static constexpr bool concurrent = true;

    template<typename X> static X load(const std::atomic<X>& c) noexcept { return c.load(std::memory_order_relaxed); }
    template<typename X> static void store(std::atomic<X>& c, std::type_identity_t<X> value) noexcept { c.store(value, std::memory_order_relaxed); }
    template<typename X> static X exchange(std::atomic<X>& c, std::type_identity_t<X> value) noexcept { return c.exchange(value, std::memory_order_acq_rel); }
    static void increment(std::atomic<W>& c, block_state&) noexcept { c.fetch_add(1, std::memory_order_relaxed); }
    static W decrement(std::atomic<W>& c, block_state&) noexcept
    {
        // TSan ignores atomic_thread_fence, and would report the destructor racing with other threads' last accesses.
        if constexpr (fm_TSAN)
            return W(c.fetch_sub(1, std::memory_order_acq_rel) - 1);
        else
        {
            W ret = W(c.fetch_sub(1, std::memory_order_release) - 1);
            if (ret == 0)
                std::atomic_thread_fence(std::memory_order_acquire);
            return ret;
        }
    }
    static bool increment_if_nonzero(std::atomic<W>& c, block_state&) noexcept
    {
        W n = c.load(std::memory_order_relaxed);
        do
            if (n == 0)
                return false;
        while (!c.compare_exchange_weak(n, W(n + 1), std::memory_order_acq_rel, std::memory_order_relaxed));
        return true;
    }
};

} // namespace floormat::bptr_policy

namespace floormat {

struct atomic_refcount : non_atomic_refcount
{
    using counter = bptr_policy::atomic_counter<uint32_t>;
};

} // namespace floormat
