//#define RTREE_POOL_DEBUG
#ifdef RTREE_POOL_DEBUG
#include <cr/StringView.h>
#include <cstdio>
namespace floormat::detail {
namespace {
size_t fresh_counter, reuse_counter, dtor_counter; // NOLINT
} // namespace
} // namespace floormat::detail
#endif
#include "RTree.hpp"
#include "rtree-pool.inl"
#include "src/object-id.hpp"

namespace floormat::detail {

template struct rtree_pool<RTree<object_id, float, 2, float>::Node>;
template struct rtree_pool<RTree<object_id, float, 2, float>::ListNode>;

} // namespace floormat::detail

using floormat::object_id;
template class RTree<object_id, float, 2, float>;
