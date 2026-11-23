//#define RTREE_POOL_DEBUG
#ifdef RTREE_POOL_DEBUG
#include <cr/StringView.h>
#include <cstdio>
namespace floormat::detail {
namespace {
usize fresh_counter, reuse_counter, dtor_counter; // NOLINT
} // namespace
} // namespace floormat::detail
#endif
#include "old-rtree.hpp"
#include "rtree-pool.inl"
#include "object-id.hpp"

namespace floormat::detail {

template struct rtree_pool<old_rtree<object_id, float, 2, float>::Node>;
template struct rtree_pool<old_rtree<object_id, float, 2, float>::ListNode>;

} // namespace floormat::detail

using floormat::object_id;
template class old_rtree<object_id, float, 2, float>;
