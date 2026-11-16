#pragma once
#include "compat/assert.hpp"
#include "point.inl"

namespace floormat::Depth {

constexpr inline u32 bias = 1u << 31;

constexpr u32 value_atʹ(point pixel)
{
    const auto pt = Vector3i(pixel);
    fm_debug3_assert(pt >= most_negative_point);
    fm_debug3_assert(pt <= most_positive_point);
    return bias + (u32)pt.sum();
}

constexpr u32 value_at(point pixel, i32 offset)
{
    return value_atʹ(pixel) + (u32)offset;
}

} // namespace floormat::Depth
