#pragma once
#include "compat/borrowed-ptr.hpp"
#include "compat/borrowed-ptr-atomic.hpp"
#include "compat/borrowed-ptr-stats.hpp"
#include "compat/assert.hpp"
#include "compat/atomic.hpp"
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#if __has_include(<sanitizer/allocator_interface.h>)
#include <sanitizer/allocator_interface.h>
#endif
#endif

namespace floormat::bptr_test {

template<typename Tag>
struct counting_allocator
{
    static inline volatile int64_t live, total;

    template<typename X> static void* allocate()
    {
        void* p = bptr_policy::new_delete_allocator::allocate<X>();
        atomic_fetch_add(&live, 1, memory_order::relaxed);
        atomic_fetch_add(&total, 1, memory_order::relaxed);
        return p;
    }
    template<typename X> static void deallocate(void* p) noexcept
    {
        atomic_fetch_sub(&live, 1, memory_order::relaxed);
        bptr_policy::new_delete_allocator::deallocate<X>(p);
    }
};

// Hands freed slots out again without telling ASan, as a real pool would.
template<typename Tag>
struct pool_allocator
{
    static constexpr bool pooling = true;
    static constexpr size_t slot_size = 256, slot_align = 64;
    struct slot { slot* next; };

    static inline slot* free_list = nullptr;
    static inline volatile int64_t live, total;

    template<typename X> static void* allocate()
    {
        void* p;
        if constexpr (sizeof(X) > slot_size || alignof(X) > slot_align)
            p = bptr_policy::new_delete_allocator::allocate<X>();
        else if (free_list)
        {
            p = free_list;
            free_list = free_list->next;
        }
        else
            p = ::operator new(slot_size, std::align_val_t{slot_align});
        atomic_fetch_add(&live, 1);
        atomic_fetch_add(&total, 1);
        return p;
    }
    template<typename X> static void deallocate(void* p) noexcept
    {
        atomic_fetch_sub(&live, 1);
        if constexpr (sizeof(X) > slot_size || alignof(X) > slot_align)
            bptr_policy::new_delete_allocator::deallocate<X>(p);
        else
            free_list = ::new (p) slot{free_list};
    }
    static void drain() noexcept
    {
        while (auto* s = free_list)
        {
            free_list = s->next;
            ::operator delete(s, std::align_val_t{slot_align});
        }
    }
};

struct counted_refcount : non_atomic_refcount
{
    using allocator = counting_allocator<counted_refcount>;
    using stats = bptr_policy::counting_stats<counted_refcount>;
};

struct small_refcount : non_atomic_refcount
{
    using counter = bptr_policy::non_atomic_counter<uint16_t>;
    using allocator = counting_allocator<small_refcount>;
    using stats = bptr_policy::counting_stats<small_refcount>;
};

struct pooled_refcount : non_atomic_refcount
{
    using allocator = pool_allocator<pooled_refcount>;
    using stats = bptr_policy::counting_stats<pooled_refcount>;
};

struct strong_refcount : non_atomic_refcount
{
    using allocator = counting_allocator<strong_refcount>;
    using stats = bptr_policy::counting_stats<strong_refcount>;
    static constexpr bool has_weak = false;
};

struct atomic_counted_refcount : atomic_refcount
{
    using allocator = counting_allocator<atomic_counted_refcount>;
    using stats = bptr_policy::counting_stats<atomic_counted_refcount>;
};

struct thread_counted_refcount : thread_checked_refcount
{
    using allocator = counting_allocator<thread_counted_refcount>;
    using stats = bptr_policy::counting_stats<thread_counted_refcount>;
};

// Only ASan exposes the heap total, so elsewhere a leaked control block goes unnoticed.
inline size_t allocated_bytes()
{
#if fm_ASAN && __has_include(<sanitizer/allocator_interface.h>)
    return __sanitizer_get_current_allocated_bytes();
#else
    return 0;
#endif
}

inline bool poisoned(const void* p)
{
#if fm_ASAN
    return __asan_address_is_poisoned(p);
#else
    (void)p;
    return true;
#endif
}

template<typename P> constexpr bool has_stats = requires { P::stats::live_blocks; };
template<typename P> constexpr bool counts_allocations = requires { P::allocator::live; };
template<typename P> constexpr bool pooling = requires { requires P::allocator::pooling; };

struct heap_state
{
    size_t bytes = 0;
    int64_t blocks = 0, objects = 0, allocations = 0;
    bool operator==(const heap_state&) const = default;
};

// Thread start-up allocates outside the test's control, so thread tests skip the heap total.
template<typename P>
heap_state heap_snapshot(bool bytes = true)
{
    heap_state s;
    if constexpr (pooling<P>)
        P::allocator::drain();
    if (bytes)
        s.bytes = allocated_bytes();
    if constexpr (has_stats<P>)
    {
        s.blocks = atomic_load(&P::stats::live_blocks);
        s.objects = atomic_load(&P::stats::live_objects);
    }
    if constexpr (counts_allocations<P>)
        s.allocations = atomic_load(&P::allocator::live);
    return s;
}

template<typename P>
void check_leaks(const heap_state& before, const char* policy, const char* what, uint32_t n, bool bytes = true)
{
    auto after = heap_snapshot<P>(bytes);
    if (after != before)
        fm_abort("bptr %s %s %u: leaked %lld bytes, %lld blocks, %lld objects, %lld allocations",
                 policy, what, n, (long long)(after.bytes - before.bytes), (long long)(after.blocks - before.blocks),
                 (long long)(after.objects - before.objects), (long long)(after.allocations - before.allocations));
}

template<typename P>
void run_test(const char* policy, uint32_t n, void(*test)())
{
    auto before = heap_snapshot<P>();
    test();
    check_leaks<P>(before, policy, "test", n);
}

} // namespace floormat::bptr_test
