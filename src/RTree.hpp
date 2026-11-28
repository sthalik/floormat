#pragma once
#include "RTree.h"
#include "compat/assert.hpp"
#include "compat/limits.hpp"
#include <bit>
#include <type_traits>
#include <mg/Functions.h>

namespace floormat {

template<typename ELEMTYPE, u32 MAXNODES>
template<typename Compare>
auto RTree<ELEMTYPE, MAXNODES>::SearchRect(const float min[2], const float max[2]) -> Rect
{
    static_assert(std::is_same_v<Compare, cmp::less_equal<>> || std::is_same_v<Compare, cmp::less<>>);
    constexpr bool is_strict = std::is_same_v<Compare, cmp::less<>>;
    constexpr i32 limit = sizeof(ELEMTYPE) == 2 ? 32766 : 1 << 30;
    static_assert(max_coordinate < limit && limit < limits<ELEMTYPE>::max);

    Vector2 lo{min[0], min[1]}, hi{max[0], max[1]};
    if constexpr(is_strict)
    {
        lo = Math::floor(lo);
        hi = Math::ceil(hi);
    }
    else
    {
        lo = Math::ceil(lo);
        hi = Math::floor(hi);
    }
    // Clamped in float, since converting a float outside i32's range is undefined. The Vector2i
    // overload moves strict bounds inward by 1 in i32, as float can't hold every integer past 2^24.
    return SearchRect<Compare>(Vector2i{Math::clamp(lo, -(float)limit, (float)limit)},
                               Vector2i{Math::clamp(hi, -(float)limit, (float)limit)});
}

template<typename ELEMTYPE, u32 MAXNODES>
template<typename Compare>
auto RTree<ELEMTYPE, MAXNODES>::SearchRect(Vector2i min, Vector2i max) -> Rect
{
    static_assert(std::is_same_v<Compare, cmp::less_equal<>> || std::is_same_v<Compare, cmp::less<>>);
    constexpr i32 inward = std::is_same_v<Compare, cmp::less<>> ? 1 : 0;
    constexpr i32 limit = sizeof(ELEMTYPE) == 2 ? 32766 : 1 << 30;
    static_assert(max_coordinate < limit && limit < limits<ELEMTYPE>::max);

    const auto lo = Math::clamp(min, -limit, limit) + Vector2i{inward};
    const auto hi = Math::clamp(max, -limit, limit) - Vector2i{inward};
    return {Math::Vector2<ELEMTYPE>{lo}, Math::Vector2<ELEMTYPE>{hi}};
}

template<typename ELEMTYPE, u32 MAXNODES>
template<typename F>
bool RTree<ELEMTYPE, MAXNODES>::Search(const Node& node, const Rect& rect, u32& num_found, F& callback)
{
    const auto bits = OverlappingBranches(node, rect);
    if (node.level > 0)
    {
        for (auto b = bits; b; b &= b - 1)
        {
            const auto index = (u32)std::countr_zero(b);
            if (!Search(*node.child[index], rect, num_found, callback))
                return false;
        }
    }
    else
    {
        for (auto b = bits; b; b &= b - 1)
        {
            const auto index = (u32)std::countr_zero(b);
            num_found++;
            const Rect r{{node.min_x[index], node.min_y[index]}, {node.max_x[index], node.max_y[index]}};
            if (!callback(node.id[index], r))
                return false;
        }
    }
    return true;
}

template<typename ELEMTYPE, u32 MAXNODES>
template<typename Compare, typename F>
u32 RTree<ELEMTYPE, MAXNODES>::Search(const float min[2], const float max[2], F&& callback) const
{
    fm_debug3_assert(min[0] <= max[0] && min[1] <= max[1]);
    u32 num_found = 0;
    Search(*root, SearchRect<Compare>(min, max), num_found, callback);
    return num_found;
}

template<typename ELEMTYPE, u32 MAXNODES>
template<typename Compare, typename F>
u32 RTree<ELEMTYPE, MAXNODES>::Search(Vector2i min, Vector2i max, F&& callback) const
{
    fm_debug3_assert((min <= max).all());
    u32 num_found = 0;
    Search(*root, SearchRect<Compare>(min, max), num_found, callback);
    return num_found;
}

} // namespace floormat
