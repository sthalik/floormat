#define RTREE_NO_EXTERN_TEMPLATE
#include "RTree.hpp"
#include "rtree-pool.inl"
#include "src/object-id.hpp"

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

template struct rtree_pool<RTree<object_id, float, 2, float>::Node>;
template struct rtree_pool<RTree<object_id, float, 2, float>::ListNode>;

} // namespace floormat::detail

template class RTree<uint64_t, float, 2, float>;
