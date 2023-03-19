#define RTREE_NO_EXTERN_TEMPLATE_POOL
#include "RTree.hpp"

//#define RTREE_POOL_DEBUG

#ifdef RTREE_POOL_DEBUG
#include <cstdio>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Debug.h>
#endif

namespace floormat::detail {

template<typename T> rtree_pool<T>::rtree_pool()
{
    free_list.reserve(128);
}

template<typename T> rtree_pool<T>::~rtree_pool()
{
    for (auto* ptr : free_list)
        ::operator delete(ptr);
    free_list.clear();
    free_list.shrink_to_fit();
}

template<typename T> T* rtree_pool<T>::construct()
{
    if (free_list.empty())
    {
#ifdef RTREE_POOL_DEBUG
        static unsigned i = 0; Debug{} << "rtree-pool: fresh"_s << ++i; std::fflush(stdout);
#endif
        return new T;
    }
    else
    {
        auto* ret = free_list.back();
        free_list.pop_back();
        new (ret) T();
#ifdef RTREE_POOL_DEBUG
        static unsigned i = 0; Debug{} << "rtree-pool: reused"_s << ++i; std::fflush(stdout);
#endif
        return ret;
    }
}

template<typename T> void rtree_pool<T>::free(T* ptr)
{
    ptr->~T();
    free_list.push_back(ptr);
}

using my_rtree = RTree<uint64_t, float, 2, float>;

template<> std::vector<my_rtree::Node*> rtree_pool<my_rtree::Node>::free_list = {}; // NOLINT
template<> std::vector<my_rtree::ListNode*> rtree_pool<my_rtree::ListNode>::free_list = {}; // NOLINT

template struct floormat::detail::rtree_pool<my_rtree::Node>;
template struct floormat::detail::rtree_pool<my_rtree::ListNode>;

} // namespace floormat::detail

template class RTree<floormat::uint64_t, float, 2, float>;
