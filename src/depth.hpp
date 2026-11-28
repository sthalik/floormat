#pragma once
#include "point.inl"

namespace floormat::Depth {

constexpr inline u32 bias = 1u << 31;

constexpr u32 value_atʹ(point pixel)
{
    return bias + (u32)Vector3i(pixel).sum();
}

constexpr u32 value_at(point pixel, i32 offset)
{
    return value_atʹ(pixel) + (u32)offset;
}

} // namespace floormat::Depth
