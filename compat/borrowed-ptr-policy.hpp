#pragma once
#include "borrowed-ptr-fwd.hpp"
#include <new>

namespace floormat::bptr_policy {

template<typename W>
struct non_atomic_counter
{
    using value_type = W;
    template<typename X> using cell = X;
    struct block_state {};
    static constexpr bool concurrent = false;

    template<typename X> static X load(const X& c) noexcept { return c; }
    template<typename X> static void store(X& c, std::type_identity_t<X> value) noexcept { c = value; }
    template<typename X> static X exchange(X& c, std::type_identity_t<X> value) noexcept
    {
        X old = c;
        c = value;
        return old;
    }
    static void increment(W& c, block_state&) noexcept { ++c; }
    static W decrement(W& c, block_state&) noexcept { return --c; }
    static bool increment_if_nonzero(W& c, block_state&) noexcept
    {
        if (!c)
            return false;
        ++c;
        return true;
    }
};

struct thread_check_state
{
    const void* owner = nullptr;
};

// Binds the block to the calling thread on its first counting operation.
void thread_check(thread_check_state& s) noexcept;

template<typename W>
struct thread_checked_counter : non_atomic_counter<W>
{
    using block_state = thread_check_state;

    static void increment(W& c, block_state& s) noexcept
    {
        thread_check(s);
        ++c;
    }
    static W decrement(W& c, block_state& s) noexcept
    {
        thread_check(s);
        return --c;
    }
    static bool increment_if_nonzero(W& c, block_state& s) noexcept
    {
        thread_check(s);
        if (!c)
            return false;
        ++c;
        return true;
    }
};

struct new_delete_allocator
{
    template<typename X> static void* allocate()
    {
        if constexpr (alignof(X) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            return ::operator new(sizeof(X), std::align_val_t{alignof(X)});
        else
            return ::operator new(sizeof(X));
    }
    template<typename X> static void deallocate(void* p) noexcept
    {
#ifdef __cpp_sized_deallocation
        if constexpr (alignof(X) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            ::operator delete(p, sizeof(X), std::align_val_t{alignof(X)});
        else
            ::operator delete(p, sizeof(X));
#else
        if constexpr (alignof(X) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            ::operator delete(p, std::align_val_t{alignof(X)});
        else
            ::operator delete(p);
#endif
    }
};

struct no_stats
{
    static void block_allocated() noexcept {}
    static void block_deallocated() noexcept {}
    static void object_constructed() noexcept {}
    static void object_disposed() noexcept {}
    static void copied() noexcept {}
    static void released() noexcept {}
};

} // namespace floormat::bptr_policy

namespace floormat {

struct non_atomic_refcount
{
    using counter = bptr_policy::non_atomic_counter<uint32_t>;
    using allocator = bptr_policy::new_delete_allocator;
    using stats = bptr_policy::no_stats;
    static constexpr bool has_weak = true;
};

struct thread_checked_refcount : non_atomic_refcount
{
    using counter = bptr_policy::thread_checked_counter<uint32_t>;
};

} // namespace floormat
