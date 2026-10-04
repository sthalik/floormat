#pragma once
#include "borrowed-ptr-policy.hpp"
#include "atomic.hpp"

namespace floormat::bptr_policy {

template<typename Tag>
struct counting_stats
{
    static inline volatile int64_t live_blocks, live_objects, copies, releases;

    static void block_allocated() noexcept { atomic_fetch_add(&live_blocks, 1, memory_order::relaxed); }
    static void block_deallocated() noexcept { atomic_fetch_sub(&live_blocks, 1, memory_order::relaxed); }
    static void object_constructed() noexcept { atomic_fetch_add(&live_objects, 1, memory_order::relaxed); }
    static void object_disposed() noexcept { atomic_fetch_sub(&live_objects, 1, memory_order::relaxed); }
    static void copied() noexcept { atomic_fetch_add(&copies, 1, memory_order::relaxed); }
    static void released() noexcept { atomic_fetch_add(&releases, 1, memory_order::relaxed); }
};

} // namespace floormat::bptr_policy
