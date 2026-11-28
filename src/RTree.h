#pragma once
#include "rtree-pool.hpp"
#include "compat/cmp.hpp"
#include "compat/defs.hpp"
#include "compat/limits.hpp"
#include <bit>
#include <type_traits>
#include <mg/Range.h>

namespace floormat {

template<typename ELEMTYPE, u32 MAXNODES>
struct RTree final
{
    static_assert(std::is_same_v<ELEMTYPE, i16> || std::is_same_v<ELEMTYPE, i32>);
    static_assert(std::has_single_bit(MAXNODES));
    static_assert(MAXNODES >= 4 && MAXNODES <= 64);

    static constexpr u32 MINNODES = MAXNODES / 2;

    using Rect = Math::Range2D<ELEMTYPE>;
    using branch_mask = std::conditional_t<(MAXNODES <= 32), u32, u64>;

    static constexpr Rect empty_rect = {Math::Vector2<ELEMTYPE>{limits<ELEMTYPE>::max},
                                        Math::Vector2<ELEMTYPE>{limits<ELEMTYPE>::min}};
    static constexpr ELEMTYPE max_coordinate = (ELEMTYPE)(sizeof(ELEMTYPE) == 2 ? 16384 : 1 << 29);

    struct Node
    {
        union { Node* child[MAXNODES]; u64 id[MAXNODES]; };
        ELEMTYPE min_x[MAXNODES], min_y[MAXNODES];
        ELEMTYPE max_x[MAXNODES], max_y[MAXNODES];
        u32 branch_count, level;
    };

    struct ListNode
    {
        ListNode* next;
        Node* node;
    };

    RTree();
    RTree(const RTree& other);
    RTree& operator=(const RTree& other);
    ~RTree() noexcept;

    void Insert(const Rect& rect, u64 id);
    void Remove(const Rect& rect, u64 id);
    bool Move(const Rect& old_rect, u64 old_id, const Rect& new_rect, u64 new_id);

    template<typename Compare = cmp::less_equal<>, typename F>
    u32 Search(const float min[2], const float max[2], F&& callback) const;
    template<typename Compare = cmp::less_equal<>, typename F>
    u32 Search(Vector2i min, Vector2i max, F&& callback) const;

    void RemoveAll();
    u32 Count() const;
    const Node* raw_root() const noexcept;

    static branch_mask OverlappingBranches(const Node& node, const Rect& rect);
    static branch_mask ContainingBranches(const Node& node, const Rect& rect);
    static u32 PickBranch(const Rect& rect, const Node& node);
    template<typename Compare> static Rect SearchRect(const float min[2], const float max[2]);
    template<typename Compare> static Rect SearchRect(Vector2i min, Vector2i max);

    static inline detail::rtree_pool<Node> node_pool;
    static inline detail::rtree_pool<ListNode> list_node_pool;

private:
    struct Branch
    {
        Rect rect;
        Node* child = nullptr;
        u64 id = 0;
    };

    struct PartitionVars
    {
        static constexpr int NOT_TAKEN = -1;

        int partition[MAXNODES + 1];
        u32 total;
        u32 min_fill;
        u32 count[2];
        Rect cover[2];
        i64 area[2];
        Branch branch_buf[MAXNODES + 1];
    };

    enum class MoveResult : u8 { not_found, removed, moved };

    static Node* AllocNode();
    static void FreeNode(Node* node);
    static void InitNode(Node& node);
    static Branch GetBranch(const Node& node, u32 index);
    static void SetBranch(Node& node, u32 index, const Branch& branch);
    static bool InsertRectRec(const Branch& branch, Node* node, Node** new_node, u32 level);
    static bool InsertRect(const Branch& branch, Node** root, u32 level);
    static Rect NodeCover(const Node& node);
    static bool AddBranch(const Branch& branch, Node& node, Node** new_node);
    static void DisconnectBranch(Node& node, u32 index);
    static Rect CombineRect(const Rect& a, const Rect& b);
    static void SplitNode(Node& node, const Branch& branch, Node** new_node);
    static i64 CalcRectVolume(const Rect& rect);
    static void GetBranches(const Node& node, const Branch& branch, PartitionVars& vars);
    static void ChoosePartition(PartitionVars& vars, u32 min_fill);
    static void LoadNodes(Node& a, Node& b, const PartitionVars& vars);
    static void InitParVars(PartitionVars& vars, u32 max_rects, u32 min_fill);
    static void PickSeeds(PartitionVars& vars);
    static void Classify(u32 index, u32 group, PartitionVars& vars);
    static bool RemoveRect(const Rect& rect, u64 id, Node** root);
    [[fm_flatten]] static bool RemoveRectRec(const Rect& rect, u64 id, Node* node, ListNode** list);
    static void FinishRemove(ListNode* list, Node** root);
    [[fm_flatten]] static MoveResult MoveRec(const Rect& rect, u64 id, const Branch& new_branch, Node* node,
                                             const Rect* bound, ListNode** list);
    static bool Contains(const Rect& outer, const Rect& inner);
    static branch_mask OccupiedBranches(const Node& node);
    static void ReInsert(Node* node, ListNode** list);
    static void RemoveAllRec(Node* node);
    static void CountRec(const Node& node, u32& count);
    static void CopyRec(Node& current, const Node& other);
    template<typename F>
    static bool Search(const Node& node, const Rect& rect, u32& num_found, F& callback);

    Node* root;
};

} // namespace floormat
