#pragma once
#include "search-constants.hpp"
#include "point.inl"
#include <mg/Functions.h>

namespace floormat::Search {

constexpr uint32_t octile_distance(point cur, point goal)
{
    const auto d = Vector2ui{Math::abs(cur - goal)};
    const uint32_t dx = d.x();
    const uint32_t dy = d.y();
    const uint32_t mn = dx < dy ? dx : dy;
    const uint32_t mx = dx > dy ? dx : dy;
    // step costs must match search-astar.cpp directions[]
    constexpr uint32_t axial = (uint32_t)div_size.x();
    constexpr uint32_t diag  = (uint32_t)(div_size.length() + 1);
    return mx + (diag - axial) * mn / axial;
}

} // namespace floormat::Search
