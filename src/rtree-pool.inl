#pragma once
#include "rtree-pool.hpp"
#include "compat/assert.hpp"
#include <new>

namespace floormat::detail {

template<typename T> rtree_pool<T>::rtree_pool() noexcept = default;

template<typename T> rtree_pool<T>::~rtree_pool() noexcept
{
    fm_assert(live_count == 0);
#ifdef RTREE_POOL_DEBUG
    auto last = dtor_counter;
#endif
    while (free_list)
    {
#ifdef RTREE_POOL_DEBUG
        ++dtor_counter;
#endif
        auto* p = free_list;
        free_list = free_list->next;
        delete p;
    }
#ifdef RTREE_POOL_DEBUG
    if (dtor_counter != last) { Debug{} << "rtree-pool: dtor" << dtor_counter; std::fflush(stdout); }
#endif
}

template<typename T> T*
rtree_pool<T>::construct()
noexcept(std::is_nothrow_default_constructible_v<T>)
{
    live_count++;

    if (!free_list)
    {
#ifdef RTREE_POOL_DEBUG
        Debug{} << "rtree-pool: fresh"_s << ++fresh_counter; std::fflush(stdout);
#endif
        node_u* n;
        n =  new node_u;
        if constexpr(std::is_nothrow_default_constructible_v<T>)
            return new (&n->data) T;
        else
            try {
                return new (&n->data) T;
            } catch (...) {
                delete n;
                throw;
            }
    }
    else [[likely]]
    {
#ifdef RTREE_POOL_DEBUG
        Debug{} << "rtree-pool: reused"_s << ++reuse_counter; std::fflush(stdout);
#endif
        auto* n = free_list;
        free_list = free_list->next;
        if constexpr(std::is_nothrow_default_constructible_v<T>)
            return new (&n->data) T;
        else
            try {
                return new (&n->data) T;
            } catch (...) {
                n->next = free_list;
                free_list = n;
                throw;
            }
    }
}

template<typename T>
void rtree_pool<T>::free(T* ptr)
noexcept(std::is_nothrow_destructible_v<T>)
{
    fm_assert(ptr);
    live_count--;
    auto* n = std::launder(reinterpret_cast<node_u*>(ptr));

    if constexpr(noexcept(std::is_nothrow_destructible_v<T>))
        ptr->~T();
    else
        try {
            ptr->~T();
        } catch (...) {
            delete n;
            throw;
        }
    n->next = free_list;
    free_list = n;
}

} // namespace floormat::detail
