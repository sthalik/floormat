#pragma once
#include "compat/assert.hpp"
#include "compat/float.hpp"
#include "compat/limits.hpp"
#include "point.inl"
#include <cfloat>

namespace floormat { struct point; }

namespace floormat::Depth {

constexpr inline float startʹ = 1.1920929e-7f;
constexpr inline float start  = -1 + startʹ; // todo clipcontrol!

constexpr uint32_t value_atʹ(point pixel)
{
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
    auto sum = x.sum();

    return sum;
}

constexpr float value_at(point pixel)
{
    auto i = value_atʹ(pixel);
    float val = nth_float(-1.f, 2*i);
    return val;
}

} // namespace floormat::Depth
