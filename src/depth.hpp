#pragma once
#include "compat/assert.hpp"
#include "compat/float.hpp"
#include "compat/limits.hpp"
#include "point.inl"

namespace floormat::Depth {

// largest per-pixel ULP step that keeps the depth float below 1.0 across the full
// coordinate extent (most_positive_point). M=8 would overflow at the SE corner.
constexpr u32 depth_step = 7;

constexpr u32 value_atʹ(point pixel)
{
    constexpr u32 extra_spacing = 4096;
    auto pt = Vector3i(pixel);
    // checked_sub equivalent
    static_assert(most_negative_point <= Vector3i{});
    static_assert(most_positive_point > most_negative_point);
    fm_debug3_assert(pt >= most_negative_point);
    fm_debug3_assert(pt <= most_positive_point);
    auto x = Vector3ui(pt - most_negative_point);

    // checked_add twice
    fm_debug3_assert(x.x() <= limits<u32>::max - x.y());
    fm_debug3_assert(x.x() + x.y() <=limits<u32>::max - x.z());
    auto sumʹ = x.sum();
    auto sum = sumʹ + extra_spacing;
    return sum;
}

constexpr float value_at(float start, u32 pixel, i32 offset)
{
    auto i = (i32)pixel;
    i += offset;
    fm_debug3_assert(i >= 0);
    float val = nth_float(start, depth_step*(u32)i);
    return val;
}

constexpr float value_at(float start, point pixel, i32 offset)
{
    auto i = value_atʹ(pixel);
    return value_at(start, i, offset);
}

} // namespace floormat::Depth
