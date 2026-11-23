#pragma once
#include "rtree-pool.hpp"
#include "compat/assert.hpp"
#include "compat/defs.hpp"
#include <cr/Debug.h>
#include <new>
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#endif

namespace floormat::detail {

template<typename T> rtree_pool<T>::rtree_pool() noexcept = default;

template<typename T> void rtree_pool<T>::poison([[maybe_unused]] node_u* n) noexcept
{
#if fm_ASAN
    __asan_poison_memory_region(n, sizeof(node_u));
#endif
}

template<typename T> void rtree_pool<T>::unpoison([[maybe_unused]] node_u* n) noexcept
{
#if fm_ASAN
    __asan_unpoison_memory_region(n, sizeof(node_u));
#endif
}

template<typename T> usize rtree_pool<T>::alive_count() const
{
    Locker<Spinlock> guard{pool_lock};
    return live_count;
}

template<typename T> rtree_pool<T>::~rtree_pool() noexcept
{
    Locker<Spinlock> guard{pool_lock};
    fm_assert_equal(0zu, live_count);
#ifdef RTREE_POOL_DEBUG
    auto last = dtor_counter;
#endif
    while (free_list)
    {
#ifdef RTREE_POOL_DEBUG
        ++dtor_counter;
#endif
        auto* p = free_list;
        unpoison(p);
        free_list = p->next;
        delete p;
    }
#ifdef RTREE_POOL_DEBUG
    if (dtor_counter != last) { Debug{} << "rtree-pool: dtor" << dtor_counter; std::fflush(stdout); }
#endif
}

template<typename T>
template<typename... Xs>
T* rtree_pool<T>::construct(Xs&&... args)
noexcept(noexcept(T{std::declval<Xs>()...}))
{
    node_u* n;
    {
        Locker<Spinlock> guard{pool_lock};
        if (!free_list) [[unlikely]]
        {
            constexpr u32 alloc_batch = 1024;

            for (auto i = 0u; i < alloc_batch; i++)
            {
                n = new node_u;
                n->next = free_list;
                free_list = n;
                poison(n);
            }

#ifdef RTREE_POOL_DEBUG
            Debug{} << "rtree-pool: fresh"_s << ++fresh_counter; std::fflush(stdout);
#endif
        }
#ifdef RTREE_POOL_DEBUG
        else
        {
            Debug{} << "rtree-pool: reused"_s << ++reuse_counter; std::fflush(stdout);
        }
#endif
        n = free_list;
        unpoison(n);
        free_list = n->next;
        live_count++;
    }

    if constexpr(noexcept(T{std::declval<Xs>()...}))
        return new (&n->data) T{forward<Xs>(args)...};
    else
        try {
            return new (&n->data) T{forward<Xs>(args)...};
        } catch (...) {
            Locker<Spinlock> guard{pool_lock};
            live_count--;
            n->next = free_list;
            free_list = n;
            poison(n);
            throw;
        }
}

template<typename T>
void rtree_pool<T>::free(T* ptr)
noexcept(std::is_nothrow_destructible_v<T>)
{
    fm_assert(ptr);
    auto* n = std::launder(reinterpret_cast<node_u*>(ptr));

    if constexpr(std::is_nothrow_destructible_v<T>)
        ptr->~T();
    else
        try {
            ptr->~T();
        } catch (...) {
            {
                Locker<Spinlock> guard{pool_lock};
                live_count--;
            }
            delete n;
            throw;
        }
    Locker<Spinlock> guard{pool_lock};
    live_count--;
    n->next = free_list;
    free_list = n;
    poison(n);
}

} // namespace floormat::detail
