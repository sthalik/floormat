#pragma once
#include "borrowed-ptr-policy.hpp"
#include "atomic.hpp"
#include "defs.hpp"

namespace floormat::bptr_policy {

template<typename W>
struct atomic_counter
{
    using value_type = W;
    template<typename X> using cell = volatile X;
    struct block_state {};
    static constexpr bool concurrent = true;

    template<typename X> static X load(const volatile X& c) noexcept { return atomic_load(&c, memory_order::relaxed); }
    template<typename X> static void store(volatile X& c, std::type_identity_t<X> value) noexcept { atomic_store(&c, value, memory_order::relaxed); }
    template<typename X> static X exchange(volatile X& c, std::type_identity_t<X> value) noexcept { return atomic_exchange(&c, value, memory_order::acq_rel); }
    static void increment(volatile W& c, block_state&) noexcept { atomic_fetch_add(&c, 1, memory_order::relaxed); }
    static W decrement(volatile W& c, block_state&) noexcept
    {
        // TSan ignores atomic_thread_fence, and would report the destructor racing with other threads' last accesses.
        if constexpr (fm_TSAN)
            return W(atomic_fetch_sub(&c, 1, memory_order::acq_rel) - 1);
        else
        {
            W ret = W(atomic_fetch_sub(&c, 1, memory_order::release) - 1);
            if (ret == 0)
                atomic_thread_fence(memory_order::acquire);
            return ret;
        }
    }
    static bool increment_if_nonzero(volatile W& c, block_state&) noexcept
    {
        W n = atomic_load(&c, memory_order::relaxed);
        do
            if (n == 0)
                return false;
        while (!atomic_compare_exchange_weak(&c, n, W(n + 1), memory_order::acq_rel));
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
