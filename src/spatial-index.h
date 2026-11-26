#pragma once
#include "RTree.h"
#include <bit>
#include <cr/Array.h>
#include <cr/ArrayView.h>

namespace floormat {

template<typename ELEMTYPE>
struct alignas(64) entry_block final
{
    static constexpr u32 capacity = 8;

    ELEMTYPE min_x[capacity], min_y[capacity];
    ELEMTYPE max_x[capacity], max_y[capacity];
    u64 id[capacity];
};

template<typename ELEMTYPE, u32 max_entries_before_tree>
struct spatial_index final
{
    using tree_type = RTree<ELEMTYPE, 32>;
    using Rect = tree_type::Rect;
    using block_type = entry_block<ELEMTYPE>;

    static constexpr u32 max_blocks =
        std::bit_ceil((max_entries_before_tree + block_type::capacity - 1) / block_type::capacity);

    spatial_index();

    void Insert(const Rect& rect, u64 id);
    void Remove(const Rect& rect, u64 id);
    bool Move(const Rect& old_rect, u64 old_id, const Rect& new_rect, u64 new_id);

    template<typename Compare = cmp::less_equal<>, typename F>
    u32 Search(const float min[2], const float max[2], F&& callback) const;
    template<typename Compare = cmp::less_equal<>, typename F>
    u32 Search(Vector2i min, Vector2i max, F&& callback) const;

    void RemoveAll();
    u32 Count() const;
    bool is_using_tree() const noexcept;
    ArrayView<const block_type> raw_blocks() const noexcept;

    static u32 OverlappingEntries(const block_type& block, const Rect& rect);

private:
    static void InitBlock(block_type& block);
    static void SetEntry(block_type& block, u32 i, const Rect& rect, u64 id);
    u32 FindEntry(const Rect& rect, u64 id) const;
    template<typename F> u32 SearchBlocks(const Rect& rect, F& callback) const;

    Array<block_type> _blocks;
    tree_type _tree;
    Rect _bounding_rect = tree_type::empty_rect;
    u32 _num_entries_in_blocks = 0;
    bool _is_using_tree = false;
};

} // namespace floormat
