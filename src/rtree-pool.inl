#pragma once
#include "rtree-pool.hpp"
#include <new>

namespace floormat::detail {

template<typename T> rtree_pool<T>::rtree_pool() noexcept = default;

template<typename T> rtree_pool<T>::~rtree_pool() noexcept
{
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

template<typename T> T* rtree_pool<T>::construct()
{
    if (!free_list)
    {
#ifdef RTREE_POOL_DEBUG
        Debug{} << "rtree-pool: fresh"_s << ++fresh_counter; std::fflush(stdout);
#endif
        node_u* n;
        n =  new node_u;
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
    auto* n = reinterpret_cast<node_u*>(ptr);
    try {
        ptr->~T();
    } catch (...) {
        n->next = free_list;
        free_list = n;
        throw;
    }
    n->next = free_list;
    free_list = n;
}

} // namespace floormat::detail
