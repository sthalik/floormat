#define RTREE_NO_EXTERN_TEMPLATE_POOL
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

template<typename T>
rtree_pool<T>::~rtree_pool()
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
        p->data.~T();
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
        auto* ptr = new node_u;
        auto* ret = new(&ptr->data) T;
        return ret;
    }
    else
    {
        auto* ret = free_list;
        free_list = free_list->next;
        new (&ret->data) T();
#ifdef RTREE_POOL_DEBUG
        Debug{} << "rtree-pool: reused"_s << ++reuse_counter; std::fflush(stdout);
#endif
        return &ret->data;
    }
}

template<typename T> void rtree_pool<T>::free(T* ptr)
{
    ptr->~T();
    node_p p = {.ptr = ptr };
    node_u* n = p.data_ptr;
    n->next = free_list;
    free_list = n;
}

using my_rtree = RTree<uint64_t, float, 2, float>;
template struct rtree_pool<my_rtree::Node>;
template struct rtree_pool<my_rtree::ListNode>;

} // namespace floormat::detail

template class RTree<floormat::uint64_t, float, 2, float>;
