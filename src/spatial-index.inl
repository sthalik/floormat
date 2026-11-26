#pragma once
#include "spatial-index.h"
#include "compat/assert.hpp"
#ifdef _MSC_VER
#include "compat/unroll.hpp"
#endif
#include <type_traits>
#include <utility>
#include <cr/Algorithms.h>
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

HWY_BEFORE_NAMESPACE();
namespace floormat::HWY_NAMESPACE {
namespace {

namespace hn = hwy::HWY_NAMESPACE;

template<typename ELEMTYPE>
[[fm_always_inline]]
u32 OverlappingEntries(const entry_block<ELEMTYPE>& block, const Math::Range2D<ELEMTYPE>& rect)
{
    static_assert(std::is_signed_v<ELEMTYPE>);
    constexpr u32 capacity = entry_block<ELEMTYPE>::capacity;
#if HWY_MAX_BYTES <= 64
    constexpr hn::CappedTag<ELEMTYPE, Math::min(capacity, 16 / (u32)sizeof(ELEMTYPE))> d;
    constexpr auto N = (u32)hn::MaxLanes(d);
    static_assert(capacity % N == 0);
    const auto min_x = hn::Set(d, rect.min().x()), min_y = hn::Set(d, rect.min().y());
    const auto max_x = hn::Set(d, rect.max().x()), max_y = hn::Set(d, rect.max().y());
    u32 disjoint_bits = 0;
#ifdef _MSC_VER
    unroll<capacity / N>([&](auto i) HWY_ATTR {
        const auto disjoint = hn::Or(hn::Or(hn::Gt(hn::Load(d, block.min_x + i * N), max_x),
                                            hn::Gt(hn::Load(d, block.min_y + i * N), max_y)),
                                     hn::Or(hn::Lt(hn::Load(d, block.max_x + i * N), min_x),
                                            hn::Lt(hn::Load(d, block.max_y + i * N), min_y)));
        disjoint_bits |= (u32)hn::BitsFromMask(d, disjoint) << (i * N);
    });
#else
    for (u32 i = 0; i < capacity; i += N)
    {
        const auto disjoint = hn::Or(hn::Or(hn::Gt(hn::Load(d, block.min_x + i), max_x),
                                            hn::Gt(hn::Load(d, block.min_y + i), max_y)),
                                     hn::Or(hn::Lt(hn::Load(d, block.max_x + i), min_x),
                                            hn::Lt(hn::Load(d, block.max_y + i), min_y)));
        disjoint_bits |= (u32)hn::BitsFromMask(d, disjoint) << i;
    }
#endif
    return ~disjoint_bits & ((1u << capacity) - 1);
#else
    u32 bits = 0;
    for (u32 i = 0; i < capacity; i++)
        if (block.min_x[i] <= rect.max().x() && block.min_y[i] <= rect.max().y() &&
            block.max_x[i] >= rect.min().x() && block.max_y[i] >= rect.min().y())
            bits |= 1u << i;
    return bits;
#endif
}

} // namespace
} // namespace floormat::HWY_NAMESPACE
HWY_AFTER_NAMESPACE();

namespace floormat {

template<typename ELEMTYPE, u32 max_entries_before_tree>
spatial_index<ELEMTYPE, max_entries_before_tree>::spatial_index() : _blocks{ValueInit, 1}
{
    InitBlock(_blocks[0]);
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
void spatial_index<ELEMTYPE, max_entries_before_tree>::Insert(const Rect& rect, u64 id)
{
    if (_is_using_tree)
        _tree.Insert(rect, id);
    else if (_num_entries_in_blocks < max_entries_before_tree)
    {
        fm_debug3_assert((rect.min() <= rect.max()).all());
        fm_debug3_assert((rect.min() >= Math::Vector2<ELEMTYPE>{-tree_type::max_coordinate}).all() &&
                         (rect.max() <= Math::Vector2<ELEMTYPE>{tree_type::max_coordinate}).all());
        const auto k = _num_entries_in_blocks / block_type::capacity;
        if (k == _blocks.size())
        {
            Array<block_type> blocks{ValueInit, Math::min(2 * (u32)_blocks.size(), max_blocks)};
            Utility::copy(_blocks, blocks.prefix(_blocks.size()));
            for (u32 j = (u32)_blocks.size(); j < blocks.size(); j++)
                InitBlock(blocks[j]);
            _blocks = std::move(blocks);
        }
        SetEntry(_blocks[k], _num_entries_in_blocks % block_type::capacity, rect, id);
        _bounding_rect = {Math::min(_bounding_rect.min(), rect.min()),
                          Math::max(_bounding_rect.max(), rect.max())};
        _num_entries_in_blocks++;
    }
    else
    {
        for (u32 j = 0; j < _num_entries_in_blocks; j++)
        {
            const auto& block = _blocks[j / block_type::capacity];
            const auto i = j % block_type::capacity;
            _tree.Insert({{block.min_x[i], block.min_y[i]}, {block.max_x[i], block.max_y[i]}}, block.id[i]);
        }
        for (u32 k = 0; k * block_type::capacity < _num_entries_in_blocks; k++)
            InitBlock(_blocks[k]);
        _num_entries_in_blocks = 0;
        _is_using_tree = true;
        _tree.Insert(rect, id);
    }
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
void spatial_index<ELEMTYPE, max_entries_before_tree>::Remove(const Rect& rect, u64 id)
{
    if (_is_using_tree)
        _tree.Remove(rect, id);
    else
    {
        fm_debug3_assert((rect.min() <= rect.max()).all());
        const auto pos = FindEntry(rect, id);
        if (pos != (u32)-1)
        {
            const auto last = _num_entries_in_blocks - 1;
            auto& from = _blocks[last / block_type::capacity];
            const auto j = last % block_type::capacity;
            SetEntry(_blocks[pos / block_type::capacity], pos % block_type::capacity,
                     {{from.min_x[j], from.min_y[j]}, {from.max_x[j], from.max_y[j]}}, from.id[j]);
            SetEntry(from, j, tree_type::empty_rect, 0);
            _num_entries_in_blocks = last;
        }
    }
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
bool spatial_index<ELEMTYPE, max_entries_before_tree>::Move(const Rect& old_rect, u64 old_id,
                                                            const Rect& new_rect, u64 new_id)
{
    if (_is_using_tree)
        return _tree.Move(old_rect, old_id, new_rect, new_id);
    else
    {
        fm_debug3_assert((old_rect.min() <= old_rect.max()).all());
        fm_debug3_assert((new_rect.min() <= new_rect.max()).all());
        fm_debug3_assert((new_rect.min() >= Math::Vector2<ELEMTYPE>{-tree_type::max_coordinate}).all() &&
                         (new_rect.max() <= Math::Vector2<ELEMTYPE>{tree_type::max_coordinate}).all());
        const auto pos = FindEntry(old_rect, old_id);
        if (pos == (u32)-1)
            return false;
        else
        {
            SetEntry(_blocks[pos / block_type::capacity], pos % block_type::capacity, new_rect, new_id);
            _bounding_rect = {Math::min(_bounding_rect.min(), new_rect.min()),
                              Math::max(_bounding_rect.max(), new_rect.max())};
            return true;
        }
    }
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
void spatial_index<ELEMTYPE, max_entries_before_tree>::RemoveAll()
{
    if (_is_using_tree)
    {
        _tree.RemoveAll();
        _is_using_tree = false;
    }
    else
        for (u32 k = 0; k * block_type::capacity < _num_entries_in_blocks; k++)
            InitBlock(_blocks[k]);
    _num_entries_in_blocks = 0;
    _bounding_rect = tree_type::empty_rect;
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
u32 spatial_index<ELEMTYPE, max_entries_before_tree>::Count() const
{
    return _is_using_tree ? _tree.Count() : _num_entries_in_blocks;
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
bool spatial_index<ELEMTYPE, max_entries_before_tree>::is_using_tree() const noexcept
{
    return _is_using_tree;
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
auto spatial_index<ELEMTYPE, max_entries_before_tree>::raw_blocks() const noexcept -> ArrayView<const block_type>
{
    return _blocks;
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
[[fm_always_inline]]
u32 spatial_index<ELEMTYPE, max_entries_before_tree>::OverlappingEntries(const block_type& block, const Rect& rect)
{
    return HWY_NAMESPACE::OverlappingEntries<ELEMTYPE>(block, rect);
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
void spatial_index<ELEMTYPE, max_entries_before_tree>::SetEntry(block_type& block, u32 i, const Rect& rect, u64 id)
{
    block.min_x[i] = rect.min().x();
    block.min_y[i] = rect.min().y();
    block.max_x[i] = rect.max().x();
    block.max_y[i] = rect.max().y();
    block.id[i] = id;
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
void spatial_index<ELEMTYPE, max_entries_before_tree>::InitBlock(block_type& block)
{
    for (u32 i = 0; i < block_type::capacity; i++)
        SetEntry(block, i, tree_type::empty_rect, 0);
}

template<typename ELEMTYPE, u32 max_entries_before_tree>
u32 spatial_index<ELEMTYPE, max_entries_before_tree>::FindEntry(const Rect& rect, u64 id) const
{
    for (u32 k = (_num_entries_in_blocks + block_type::capacity - 1) / block_type::capacity; k-- > 0;)
    {
        const auto& block = _blocks[k];
        for (auto b = OverlappingEntries(block, rect); b; b &= b - 1)
        {
            const auto i = (u32)std::countr_zero(b);
            if (block.id[i] == id)
                return k * block_type::capacity + i;
        }
    }
    return (u32)-1;
}

} // namespace floormat

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
