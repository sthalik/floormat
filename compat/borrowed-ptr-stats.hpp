#pragma once
#include "borrowed-ptr-policy.hpp"
#include <atomic>

namespace floormat::bptr_policy {

template<typename Tag>
struct counting_stats
{
    static inline std::atomic<int64_t> live_blocks, live_objects, copies, releases;

    static void block_allocated() noexcept { live_blocks.fetch_add(1, std::memory_order_relaxed); }
    static void block_deallocated() noexcept { live_blocks.fetch_sub(1, std::memory_order_relaxed); }
    static void object_constructed() noexcept { live_objects.fetch_add(1, std::memory_order_relaxed); }
    static void object_disposed() noexcept { live_objects.fetch_sub(1, std::memory_order_relaxed); }
    static void copied() noexcept { copies.fetch_add(1, std::memory_order_relaxed); }
    static void released() noexcept { releases.fetch_add(1, std::memory_order_relaxed); }
};

} // namespace floormat::bptr_policy
