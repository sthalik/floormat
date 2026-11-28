#include "RTree.inl"

namespace floormat {

namespace detail {
template struct rtree_pool<RTree<i16, 32>::Node>;
template struct rtree_pool<RTree<i16, 32>::ListNode>;
template struct rtree_pool<RTree<i32, 32>::Node>;
template struct rtree_pool<RTree<i32, 32>::ListNode>;
} // namespace detail

template struct RTree<i16, 32>;
template struct RTree<i32, 32>;

} // namespace floormat
