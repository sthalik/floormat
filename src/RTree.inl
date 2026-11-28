#pragma once
#include "RTree.h"
#include "rtree-pool.inl"
#include "compat/assert.hpp"
#ifdef _MSC_VER
#include "compat/unroll.hpp"
#endif
#include <mg/Functions.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#endif
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#endif
#include <hwy/highway.h>

#if defined __GNUC__ && !defined __clang__
#pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace floormat {

template<typename ELEMTYPE, u32 MAXNODES>
RTree<ELEMTYPE, MAXNODES>::RTree()
{
    root = AllocNode();
}

template<typename ELEMTYPE, u32 MAXNODES>
RTree<ELEMTYPE, MAXNODES>::RTree(const RTree& other) : RTree()
{
    CopyRec(*root, *other.root);
}

template<typename ELEMTYPE, u32 MAXNODES>
auto RTree<ELEMTYPE, MAXNODES>::operator=(const RTree& other) -> RTree&
{
    if (&other != this)
    {
        RemoveAll();
        CopyRec(*root, *other.root);
    }
    return *this;
}

template<typename ELEMTYPE, u32 MAXNODES>
RTree<ELEMTYPE, MAXNODES>::~RTree() noexcept
{
    RemoveAllRec(root);
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::Insert(const Rect& rect, u64 id)
{
    fm_debug3_assert((rect.min() <= rect.max()).all());
    fm_debug3_assert(Contains({Math::Vector2<ELEMTYPE>{-max_coordinate}, Math::Vector2<ELEMTYPE>{max_coordinate}},
                              rect));
    InsertRect(Branch{.rect = rect, .id = id}, &root, 0);
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::Remove(const Rect& rect, u64 id)
{
    fm_debug3_assert((rect.min() <= rect.max()).all());
    RemoveRect(rect, id, &root);
}

template<typename ELEMTYPE, u32 MAXNODES>
bool RTree<ELEMTYPE, MAXNODES>::Move(const Rect& old_rect, u64 old_id, const Rect& new_rect, u64 new_id)
{
    fm_debug3_assert((old_rect.min() <= old_rect.max()).all());
    fm_debug3_assert((new_rect.min() <= new_rect.max()).all());
    fm_debug3_assert(Contains({Math::Vector2<ELEMTYPE>{-max_coordinate}, Math::Vector2<ELEMTYPE>{max_coordinate}},
                              new_rect));

    const Branch branch{.rect = new_rect, .id = new_id};
    ListNode* list = nullptr;
    const auto result = MoveRec(old_rect, old_id, branch, root, nullptr, &list);
    if (result == MoveResult::removed)
    {
        FinishRemove(list, &root);
        InsertRect(branch, &root, 0);
    }
    return result != MoveResult::not_found;
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::RemoveAll()
{
    RemoveAllRec(root);
    root = AllocNode();
}

template<typename ELEMTYPE, u32 MAXNODES>
u32 RTree<ELEMTYPE, MAXNODES>::Count() const
{
    u32 count = 0;
    CountRec(*root, count);
    return count;
}

template<typename ELEMTYPE, u32 MAXNODES>
auto RTree<ELEMTYPE, MAXNODES>::raw_root() const noexcept -> const Node*
{
    return root;
}

template<typename ELEMTYPE, u32 MAXNODES>
auto RTree<ELEMTYPE, MAXNODES>::AllocNode() -> Node*
{
    Node* node = node_pool.construct();
    InitNode(*node);
    return node;
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::FreeNode(Node* node)
{
    fm_assert(node);
    node_pool.free(node);
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::InitNode(Node& node)
{
    node.branch_count = 0;
    for (u32 i = 0; i < MAXNODES; i++)
    {
        node.min_x[i] = empty_rect.min().x();
        node.min_y[i] = empty_rect.min().y();
        node.max_x[i] = empty_rect.max().x();
        node.max_y[i] = empty_rect.max().y();
    }
}

template<typename ELEMTYPE, u32 MAXNODES>
auto RTree<ELEMTYPE, MAXNODES>::GetBranch(const Node& node, u32 index) -> Branch
{
    fm_debug3_assert(index < MAXNODES);
    Branch branch{.rect = {{node.min_x[index], node.min_y[index]}, {node.max_x[index], node.max_y[index]}}};
    if (node.level > 0)
        branch.child = node.child[index];
    else
        branch.id = node.id[index];
    return branch;
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::SetBranch(Node& node, u32 index, const Branch& branch)
{
    fm_debug3_assert(index < MAXNODES);
    node.min_x[index] = branch.rect.min().x();
    node.min_y[index] = branch.rect.min().y();
    node.max_x[index] = branch.rect.max().x();
    node.max_y[index] = branch.rect.max().y();
    if (node.level > 0)
        node.child[index] = branch.child;
    else
        node.id[index] = branch.id;
}

template<typename ELEMTYPE, u32 MAXNODES>
bool RTree<ELEMTYPE, MAXNODES>::InsertRectRec(const Branch& branch, Node* node, Node** new_node, u32 level)
{
    fm_assert(node && new_node);
    fm_assert(level <= node->level);

    if (node->level > level)
    {
        Node* other_node;
        const auto index = PickBranch(branch.rect, *node);
        const bool child_was_split = InsertRectRec(branch, node->child[index], &other_node, level);
        auto b = GetBranch(*node, index);
        if (!child_was_split)
        {
            b.rect = CombineRect(branch.rect, b.rect);
            SetBranch(*node, index, b);
            return false;
        }
        else
        {
            b.rect = NodeCover(*b.child);
            SetBranch(*node, index, b);
            return AddBranch(Branch{.rect = NodeCover(*other_node), .child = other_node}, *node, new_node);
        }
    }
    else
        return AddBranch(branch, *node, new_node);
}

template<typename ELEMTYPE, u32 MAXNODES>
bool RTree<ELEMTYPE, MAXNODES>::InsertRect(const Branch& branch, Node** root, u32 level)
{
    fm_assert(root);
    fm_assert(level <= (*root)->level);
    fm_debug3_assert((branch.rect.min() <= branch.rect.max()).all());

    Node* new_node;
    if (InsertRectRec(branch, *root, &new_node, level))
    {
        Node* new_root = AllocNode();
        new_root->level = (*root)->level + 1;
        AddBranch(Branch{.rect = NodeCover(**root), .child = *root}, *new_root, nullptr);
        AddBranch(Branch{.rect = NodeCover(*new_node), .child = new_node}, *new_root, nullptr);
        *root = new_root;
        return true;
    }
    else
        return false;
}

template<typename ELEMTYPE, u32 MAXNODES>
auto RTree<ELEMTYPE, MAXNODES>::NodeCover(const Node& node) -> Rect
{
    auto rect = GetBranch(node, 0).rect;
    for (u32 index = 1; index < node.branch_count; index++)
        rect = CombineRect(rect, GetBranch(node, index).rect);
    return rect;
}

template<typename ELEMTYPE, u32 MAXNODES>
bool RTree<ELEMTYPE, MAXNODES>::AddBranch(const Branch& branch, Node& node, Node** new_node)
{
    if (node.branch_count < MAXNODES)
    {
        SetBranch(node, node.branch_count, branch);
        node.branch_count++;
        return false;
    }
    else
    {
        fm_assert(new_node);
        SplitNode(node, branch, new_node);
        return true;
    }
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::DisconnectBranch(Node& node, u32 index)
{
    fm_assert(index < node.branch_count);
    const auto last = node.branch_count - 1;
    SetBranch(node, index, GetBranch(node, last));
    SetBranch(node, last, Branch{.rect = empty_rect});
    node.branch_count = last;
}

template<typename ELEMTYPE, u32 MAXNODES>
[[fm_always_inline]]
u32 RTree<ELEMTYPE, MAXNODES>::PickBranch(const Rect& rect, const Node& node)
{
    bool first_time = true;
    i64 best_incr = -1, best_area = -1;
    u32 best = 0;

    for (u32 index = 0; index < node.branch_count; index++)
    {
        const auto cur_rect = GetBranch(node, index).rect;
        const i64 area = CalcRectVolume(cur_rect);
        const i64 increase = CalcRectVolume(CombineRect(rect, cur_rect)) - area;
        if (increase < best_incr || first_time)
        {
            best = index;
            best_area = area;
            best_incr = increase;
            first_time = false;
        }
        else if (increase == best_incr && area < best_area)
        {
            best = index;
            best_area = area;
            best_incr = increase;
        }
    }
    return best;
}

template<typename ELEMTYPE, u32 MAXNODES>
auto RTree<ELEMTYPE, MAXNODES>::CombineRect(const Rect& a, const Rect& b) -> Rect
{
    return {Math::min(a.min(), b.min()), Math::max(a.max(), b.max())};
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::SplitNode(Node& node, const Branch& branch, Node** new_node)
{
    PartitionVars vars;
    GetBranches(node, branch, vars);
    ChoosePartition(vars, MINNODES);

    *new_node = AllocNode();
    (*new_node)->level = node.level;

    InitNode(node);
    LoadNodes(node, **new_node, vars);

    fm_assert(node.branch_count + (*new_node)->branch_count == vars.total);
}

template<typename ELEMTYPE, u32 MAXNODES>
i64 RTree<ELEMTYPE, MAXNODES>::CalcRectVolume(const Rect& rect)
{
    static_assert(i64{max_coordinate} * 2 <= i64{1} << 30);
    const i64 dx = (i64)rect.max().x() - rect.min().x();
    const i64 dy = (i64)rect.max().y() - rect.min().y();
    return dx*dx + dy*dy;
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::GetBranches(const Node& node, const Branch& branch, PartitionVars& vars)
{
    fm_assert(node.branch_count == MAXNODES);
    for (u32 index = 0; index < MAXNODES; index++)
        vars.branch_buf[index] = GetBranch(node, index);
    vars.branch_buf[MAXNODES] = branch;
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::ChoosePartition(PartitionVars& vars, u32 min_fill)
{
    i64 biggest_diff = 0;
    u32 chosen = 0, better_group = 0;

    InitParVars(vars, MAXNODES + 1, min_fill);
    PickSeeds(vars);

    while (vars.count[0] + vars.count[1] < vars.total &&
           vars.count[0] < vars.total - vars.min_fill &&
           vars.count[1] < vars.total - vars.min_fill)
    {
        bool first_time = true;
        for (u32 index = 0; index < vars.total; index++)
        {
            if (vars.partition[index] == PartitionVars::NOT_TAKEN)
            {
                const auto& cur_rect = vars.branch_buf[index].rect;
                const i64 growth0 = CalcRectVolume(CombineRect(cur_rect, vars.cover[0])) - vars.area[0];
                const i64 growth1 = CalcRectVolume(CombineRect(cur_rect, vars.cover[1])) - vars.area[1];
                const u32 group = growth1 - growth0 >= 0 ? 0 : 1;
                const i64 diff = Math::abs(growth1 - growth0);
                if (first_time || diff > biggest_diff)
                {
                    first_time = false;
                    biggest_diff = diff;
                    chosen = index;
                    better_group = group;
                }
                else if (diff == biggest_diff && vars.count[group] < vars.count[better_group])
                {
                    chosen = index;
                    better_group = group;
                }
            }
        }
        Classify(chosen, better_group, vars);
    }

    if (vars.count[0] + vars.count[1] < vars.total)
    {
        const u32 group = vars.count[0] >= vars.total - vars.min_fill ? 1 : 0;
        for (u32 index = 0; index < vars.total; index++)
            if (vars.partition[index] == PartitionVars::NOT_TAKEN)
                Classify(index, group, vars);
    }

    fm_assert(vars.count[0] + vars.count[1] == vars.total);
    fm_assert(vars.count[0] >= vars.min_fill && vars.count[1] >= vars.min_fill);
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::LoadNodes(Node& a, Node& b, const PartitionVars& vars)
{
    for (u32 index = 0; index < vars.total; index++)
    {
        const auto group = vars.partition[index];
        fm_assert(group == 0 || group == 1);
        const bool was_split = AddBranch(vars.branch_buf[index], group == 0 ? a : b, nullptr);
        fm_assert(!was_split);
    }
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::InitParVars(PartitionVars& vars, u32 max_rects, u32 min_fill)
{
    vars.count[0] = vars.count[1] = 0;
    vars.area[0] = vars.area[1] = 0;
    vars.total = max_rects;
    vars.min_fill = min_fill;
    for (u32 index = 0; index < max_rects; index++)
        vars.partition[index] = PartitionVars::NOT_TAKEN;
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::PickSeeds(PartitionVars& vars)
{
    bool first_time = true;
    u32 seed0 = 0, seed1 = 0;
    i64 worst = 0;
    i64 area[MAXNODES + 1];

    for (u32 index = 0; index < vars.total; index++)
        area[index] = CalcRectVolume(vars.branch_buf[index].rect);

    for (u32 index_a = 0; index_a < vars.total - 1; index_a++)
        for (u32 index_b = index_a + 1; index_b < vars.total; index_b++)
        {
            const auto one_rect = CombineRect(vars.branch_buf[index_a].rect, vars.branch_buf[index_b].rect);
            const i64 waste = CalcRectVolume(one_rect) - area[index_a] - area[index_b];
            if (first_time || waste > worst)
            {
                first_time = false;
                worst = waste;
                seed0 = index_a;
                seed1 = index_b;
            }
        }

    Classify(seed0, 0, vars);
    Classify(seed1, 1, vars);
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::Classify(u32 index, u32 group, PartitionVars& vars)
{
    fm_assert(vars.partition[index] == PartitionVars::NOT_TAKEN);
    vars.partition[index] = (int)group;
    if (vars.count[group] == 0)
        vars.cover[group] = vars.branch_buf[index].rect;
    else
        vars.cover[group] = CombineRect(vars.branch_buf[index].rect, vars.cover[group]);
    vars.area[group] = CalcRectVolume(vars.cover[group]);
    vars.count[group]++;
}

template<typename ELEMTYPE, u32 MAXNODES>
bool RTree<ELEMTYPE, MAXNODES>::RemoveRect(const Rect& rect, u64 id, Node** root)
{
    fm_assert(root && *root);
    ListNode* list = nullptr;
    if (!RemoveRectRec(rect, id, *root, &list))
    {
        FinishRemove(list, root);
        return false;
    }
    else
        return true;
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::FinishRemove(ListNode* list, Node** root)
{
    while (list)
    {
        Node* node = list->node;
        for (u32 index = 0; index < node->branch_count; index++)
            InsertRect(GetBranch(*node, index), root, node->level);
        ListNode* next = list->next;
        FreeNode(node);
        list_node_pool.free(list);
        list = next;
    }

    if ((*root)->branch_count == 1 && (*root)->level > 0)
    {
        Node* child = (*root)->child[0];
        fm_assert(child);
        FreeNode(*root);
        *root = child;
    }
}

template<typename ELEMTYPE, u32 MAXNODES>
bool RTree<ELEMTYPE, MAXNODES>::RemoveRectRec(const Rect& rect, u64 id, Node* node, ListNode** list)
{
    fm_assert(node && list);

    if (node->level > 0)
    {
        for (auto bits = OverlappingBranches(*node, rect) & OccupiedBranches(*node); bits; bits &= bits - 1)
        {
            const auto index = (u32)std::countr_zero(bits);
            Node* child = node->child[index];
            if (!RemoveRectRec(rect, id, child, list))
            {
                if (child->branch_count >= MINNODES)
                {
                    auto b = GetBranch(*node, index);
                    b.rect = NodeCover(*child);
                    SetBranch(*node, index, b);
                }
                else
                {
                    ReInsert(child, list);
                    DisconnectBranch(*node, index);
                }
                return false;
            }
        }
        return true;
    }
    else
    {
        for (u32 index = 0; index < node->branch_count; index++)
        {
            if (node->id[index] == id)
            {
                DisconnectBranch(*node, index);
                return false;
            }
        }
        return true;
    }
}

template<typename ELEMTYPE, u32 MAXNODES>
auto RTree<ELEMTYPE, MAXNODES>::MoveRec(const Rect& rect, u64 id, const Branch& new_branch, Node* node,
                                        const Rect* bound, ListNode** list) -> MoveResult
{
    fm_assert(node && list);

    if (node->level > 0)
    {
        for (auto bits = ContainingBranches(*node, rect) & OccupiedBranches(*node); bits; bits &= bits - 1)
        {
            const auto index = (u32)std::countr_zero(bits);
            auto b = GetBranch(*node, index);
            switch (MoveRec(rect, id, new_branch, b.child, &b.rect, list))
            {
            case MoveResult::not_found:
                break;
            case MoveResult::moved:
                return MoveResult::moved;
            case MoveResult::removed:
                if (b.child->branch_count >= MINNODES)
                {
                    b.rect = NodeCover(*b.child);
                    SetBranch(*node, index, b);
                }
                else
                {
                    ReInsert(b.child, list);
                    DisconnectBranch(*node, index);
                }
                return MoveResult::removed;
            }
        }
        return MoveResult::not_found;
    }
    else
    {
        for (u32 index = 0; index < node->branch_count; index++)
        {
            if (node->id[index] == id)
            {
                if (bound && !Contains(*bound, new_branch.rect))
                {
                    DisconnectBranch(*node, index);
                    return MoveResult::removed;
                }
                else
                {
                    SetBranch(*node, index, Branch{.rect = new_branch.rect, .id = new_branch.id});
                    return MoveResult::moved;
                }
            }
        }
        return MoveResult::not_found;
    }
}

template<typename ELEMTYPE, u32 MAXNODES>
bool RTree<ELEMTYPE, MAXNODES>::Contains(const Rect& outer, const Rect& inner)
{
    return (inner.min() >= outer.min()).all() && (inner.max() <= outer.max()).all();
}

template<typename ELEMTYPE, u32 MAXNODES>
auto RTree<ELEMTYPE, MAXNODES>::OccupiedBranches(const Node& node) -> branch_mask
{
    fm_debug3_assert(node.branch_count <= MAXNODES);
    if constexpr(MAXNODES < 64)
        return (branch_mask)((u64{1} << node.branch_count) - 1);
    else
        return node.branch_count == MAXNODES ? ~branch_mask{} : (branch_mask)((u64{1} << node.branch_count) - 1);
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::ReInsert(Node* node, ListNode** list)
{
    *list = list_node_pool.construct(*list, node);
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::RemoveAllRec(Node* node)
{
    fm_assert(node);
    if (node->level > 0)
        for (u32 index = 0; index < node->branch_count; index++)
            RemoveAllRec(node->child[index]);
    FreeNode(node);
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::CountRec(const Node& node, u32& count)
{
    if (node.level > 0)
        for (u32 index = 0; index < node.branch_count; index++)
            CountRec(*node.child[index], count);
    else
        count += node.branch_count;
}

template<typename ELEMTYPE, u32 MAXNODES>
void RTree<ELEMTYPE, MAXNODES>::CopyRec(Node& current, const Node& other)
{
    current.level = other.level;
    current.branch_count = other.branch_count;
    for (u32 index = 0; index < other.branch_count; index++)
    {
        auto b = GetBranch(other, index);
        if (other.level > 0)
        {
            b.child = AllocNode();
            CopyRec(*b.child, *other.child[index]);
        }
        SetBranch(current, index, b);
    }
}

} // namespace floormat

HWY_BEFORE_NAMESPACE();
namespace floormat::HWY_NAMESPACE {
namespace {

namespace hn = hwy::HWY_NAMESPACE;

template<typename ELEMTYPE, u32 MAXNODES>
[[fm_always_inline]]
auto OverlappingBranches(const typename RTree<ELEMTYPE, MAXNODES>::Node& node,
                         const typename RTree<ELEMTYPE, MAXNODES>::Rect& rect)
{
    static_assert(std::is_signed_v<ELEMTYPE>);
    using branch_mask = RTree<ELEMTYPE, MAXNODES>::branch_mask;
#if HWY_MAX_BYTES <= 64
    constexpr hn::CappedTag<ELEMTYPE, Math::min(MAXNODES, 16 / (u32)sizeof(ELEMTYPE))> d;
    constexpr auto N = (u32)hn::MaxLanes(d);
    static_assert(MAXNODES % N == 0);
    const auto min_x = hn::Set(d, rect.min().x()), min_y = hn::Set(d, rect.min().y());
    const auto max_x = hn::Set(d, rect.max().x()), max_y = hn::Set(d, rect.max().y());
    branch_mask disjoint_bits = 0;
#ifdef _MSC_VER
    unroll<MAXNODES / N>([&](auto i) HWY_ATTR {
        const auto disjoint = hn::Or(hn::Or(hn::Gt(hn::LoadU(d, node.min_x + i * N), max_x),
                                            hn::Gt(hn::LoadU(d, node.min_y + i * N), max_y)),
                                     hn::Or(hn::Lt(hn::LoadU(d, node.max_x + i * N), min_x),
                                            hn::Lt(hn::LoadU(d, node.max_y + i * N), min_y)));
        disjoint_bits |= (branch_mask)((branch_mask)hn::BitsFromMask(d, disjoint) << (i * N));
    });
#else
    for (u32 i = 0; i < MAXNODES; i += N)
    {
        const auto disjoint = hn::Or(hn::Or(hn::Gt(hn::LoadU(d, node.min_x + i), max_x),
                                            hn::Gt(hn::LoadU(d, node.min_y + i), max_y)),
                                     hn::Or(hn::Lt(hn::LoadU(d, node.max_x + i), min_x),
                                            hn::Lt(hn::LoadU(d, node.max_y + i), min_y)));
        disjoint_bits |= (branch_mask)((branch_mask)hn::BitsFromMask(d, disjoint) << i);
    }
#endif
    return (branch_mask)(~disjoint_bits & (branch_mask)(~u64{} >> (64 - MAXNODES)));
#else
    branch_mask bits = 0;
    for (u32 i = 0; i < MAXNODES; i++)
        if (node.min_x[i] <= rect.max().x() && node.min_y[i] <= rect.max().y() &&
            node.max_x[i] >= rect.min().x() && node.max_y[i] >= rect.min().y())
            bits |= (branch_mask)((branch_mask)1 << i);
    return bits;
#endif
}

template<typename ELEMTYPE, u32 MAXNODES>
[[fm_always_inline]]
auto ContainingBranches(const typename RTree<ELEMTYPE, MAXNODES>::Node& node,
                        const typename RTree<ELEMTYPE, MAXNODES>::Rect& rect)
{
    static_assert(std::is_signed_v<ELEMTYPE>);
    using branch_mask = RTree<ELEMTYPE, MAXNODES>::branch_mask;
#if HWY_MAX_BYTES <= 64
    constexpr hn::CappedTag<ELEMTYPE, Math::min(MAXNODES, 16 / (u32)sizeof(ELEMTYPE))> d;
    constexpr auto N = (u32)hn::MaxLanes(d);
    static_assert(MAXNODES % N == 0);
    const auto min_x = hn::Set(d, rect.min().x()), min_y = hn::Set(d, rect.min().y());
    const auto max_x = hn::Set(d, rect.max().x()), max_y = hn::Set(d, rect.max().y());
    branch_mask outside_bits = 0;
#ifdef _MSC_VER
    unroll<MAXNODES / N>([&](auto i) HWY_ATTR {
        const auto outside = hn::Or(hn::Or(hn::Gt(hn::LoadU(d, node.min_x + i * N), min_x),
                                           hn::Gt(hn::LoadU(d, node.min_y + i * N), min_y)),
                                    hn::Or(hn::Lt(hn::LoadU(d, node.max_x + i * N), max_x),
                                           hn::Lt(hn::LoadU(d, node.max_y + i * N), max_y)));
        outside_bits |= (branch_mask)((branch_mask)hn::BitsFromMask(d, outside) << (i * N));
    });
#else
    for (u32 i = 0; i < MAXNODES; i += N)
    {
        const auto outside = hn::Or(hn::Or(hn::Gt(hn::LoadU(d, node.min_x + i), min_x),
                                           hn::Gt(hn::LoadU(d, node.min_y + i), min_y)),
                                    hn::Or(hn::Lt(hn::LoadU(d, node.max_x + i), max_x),
                                           hn::Lt(hn::LoadU(d, node.max_y + i), max_y)));
        outside_bits |= (branch_mask)((branch_mask)hn::BitsFromMask(d, outside) << i);
    }
#endif
    return (branch_mask)(~outside_bits & (branch_mask)(~u64{} >> (64 - MAXNODES)));
#else
    branch_mask bits = 0;
    for (u32 i = 0; i < MAXNODES; i++)
        if (node.min_x[i] <= rect.min().x() && node.min_y[i] <= rect.min().y() &&
            node.max_x[i] >= rect.max().x() && node.max_y[i] >= rect.max().y())
            bits |= (branch_mask)((branch_mask)1 << i);
    return bits;
#endif
}

} // namespace
} // namespace floormat::HWY_NAMESPACE
HWY_AFTER_NAMESPACE();

namespace floormat {

template<typename ELEMTYPE, u32 MAXNODES>
[[fm_always_inline]]
auto RTree<ELEMTYPE, MAXNODES>::OverlappingBranches(const Node& node, const Rect& rect) -> branch_mask
{
    return HWY_NAMESPACE::OverlappingBranches<ELEMTYPE, MAXNODES>(node, rect);
}

template<typename ELEMTYPE, u32 MAXNODES>
[[fm_always_inline]]
auto RTree<ELEMTYPE, MAXNODES>::ContainingBranches(const Node& node, const Rect& rect) -> branch_mask
{
    return HWY_NAMESPACE::ContainingBranches<ELEMTYPE, MAXNODES>(node, rect);
}

} // namespace floormat

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
