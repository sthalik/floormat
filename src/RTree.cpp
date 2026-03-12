#define RTREE_NO_EXTERN_TEMPLATE
#include "RTree.hpp"

//#define RTREE_POOL_DEBUG

#ifdef RTREE_POOL_DEBUG
#include <cstdio>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Debug.h>
#endif

namespace floormat::detail {

#ifdef RTREE_POOL_DEBUG
static size_t fresh_counter, reuse_counter, dtor_counter; // NOLINT
#endif

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
    static_assert(offsetof(rtree_pool<T>::node_u, data) == 0);
    static_assert(offsetof(rtree_pool<T>::node_u, next) == offsetof(rtree_pool<T>::node_u, data));

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

using my_rtree = RTree<floormat::uint64_t, float, 2, float>;
template struct floormat::detail::rtree_pool<my_rtree::Node>;
template struct floormat::detail::rtree_pool<my_rtree::ListNode>;
template<> floormat::detail::rtree_pool<my_rtree::Node> my_rtree::node_pool = {}; // NOLINT
template<> floormat::detail::rtree_pool<my_rtree::ListNode> my_rtree::list_node_pool = {}; // NOLINT
template class RTree<floormat::uint64_t, float, 2, float>;
