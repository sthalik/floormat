#pragma once
#include "rtree-pool.hpp"
#include "compat/assert.hpp"
#include <cr/Debug.h>
#include <new>

namespace floormat::detail {

template<typename T> rtree_pool<T>::rtree_pool() noexcept = default;

template<typename T> size_t rtree_pool<T>::alive_count() const { return live_count; }

template<typename T> rtree_pool<T>::~rtree_pool() noexcept
{
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
        free_list = free_list->next;
        delete p;
    }
#ifdef RTREE_POOL_DEBUG
    if (dtor_counter != last) { Debug{} << "rtree-pool: dtor" << dtor_counter; std::fflush(stdout); }
#endif
}

template<typename T>
template<typename... Xs>
T* rtree_pool<T>::construct(Xs&&... args)
noexcept(std::is_nothrow_constructible_v<T, decltype(forward<Xs>(args))...>)
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
            return new (&n->data) T{forward<Xs>(args)...};
        else
            try {
                return new (&n->data) T{forward<Xs>(args)...};
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
            return new (&n->data) T{forward<Xs>(args)...};
        else
            try {
                return new (&n->data) T{forward<Xs>(args)...};
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
