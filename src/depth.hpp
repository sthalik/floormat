#pragma once
#include "compat/assert.hpp"
#include "compat/float.hpp"
#include "compat/limits.hpp"
#include "point.inl"

namespace floormat::Depth {

constexpr inline float startʹ = 1.1920929e-7f;
constexpr inline float start  = -1 + startʹ; // todo clipcontrol!

constexpr uint32_t value_atʹ(point pixel)
{
    constexpr uint32_t extra_spacing = 4096;
    auto pt = Vector3i(pixel);
    // checked_sub equivalent
    static_assert(most_negative_point <= Vector3i{});
    static_assert(most_positive_point > most_negative_point);
    fm_assert(pt >= most_negative_point);
    fm_assert(pt <= most_positive_point);
    auto x = Vector3ui(pt - most_negative_point);

    // checked_add twice
    fm_assert(x.x() <= limits<uint32_t>::max - x.y());
    fm_assert(x.x() + x.y() <=limits<uint32_t>::max - x.z());
    auto sumʹ = x.sum();
    auto sum = sumʹ + extra_spacing;
    return sum;
}

constexpr float value_at(float start, uint32_t pixel, int32_t offset = 0)
{
    auto i = (int32_t)pixel;
    i += offset;
    fm_debug2_assert(i >= 0);
    float val = nth_float(start, (uint32_t)i);
    return val;
}

constexpr float value_at(float start, point pixel, int32_t offset = 0)
{
    auto i = value_atʹ(pixel);
    return value_at(start, i, offset);
}

} // namespace floormat::Depth
