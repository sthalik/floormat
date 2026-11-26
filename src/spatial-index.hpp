#pragma once
#include "spatial-index.h"
#include "RTree.hpp"
#include "compat/assert.hpp"

namespace floormat {

template<typename ELEMTYPE, u32 max_entries_before_tree>
template<typename F>
u32 spatial_index<ELEMTYPE, max_entries_before_tree>::SearchBlocks(const Rect& rect, F& callback) const
{
    u32 num_found = 0;
    if ((rect.min() <= _bounding_rect.max()).all() && (rect.max() >= _bounding_rect.min()).all())
    {
        const u32 num_blocks = (_num_entries_in_blocks + block_type::capacity - 1) / block_type::capacity;
        for (u32 k = 0; k < num_blocks; k++)
        {
            const auto& block = _blocks[k];
            for (auto b = OverlappingEntries(block, rect); b; b &= b - 1)
            {
                const auto i = (u32)std::countr_zero(b);
                num_found++;
                const Rect r{{block.min_x[i], block.min_y[i]}, {block.max_x[i], block.max_y[i]}};
                if (!callback(block.id[i], r))
                    return num_found;
            }
        }
    }
    return num_found;
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
template<typename Compare, typename F>
u32 spatial_index<ELEMTYPE, max_entries_before_tree>::Search(const float min[2], const float max[2],
                                                             F&& callback) const
{
    if (_is_using_tree)
        return _tree.template Search<Compare>(min, max, callback);
    else
    {
        fm_debug3_assert(min[0] <= max[0] && min[1] <= max[1]);
        return SearchBlocks(tree_type::template SearchRect<Compare>(min, max), callback);
    }
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
template<typename Compare, typename F>
u32 spatial_index<ELEMTYPE, max_entries_before_tree>::Search(Vector2i min, Vector2i max, F&& callback) const
{
    if (_is_using_tree)
        return _tree.template Search<Compare>(min, max, callback);
    else
    {
        fm_debug3_assert((min <= max).all());
        return SearchBlocks(tree_type::template SearchRect<Compare>(min, max), callback);
    }
}

} // namespace floormat
