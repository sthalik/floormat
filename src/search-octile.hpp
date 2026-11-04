#pragma once
#include "point.inl"
#include <bit>
#include <mg/Constants.h>
#include <mg/Functions.h>

namespace floormat::Search {

// Step costs of a search stepping `step` px, a power of two. Must match search-astar.cpp directions.
struct octile
{
    u32 axial, diag, shift;

    explicit constexpr octile(u32 step);
    constexpr u32 operator()(Vector2i delta) const;
    constexpr u32 operator()(point cur, point goal) const;
};

constexpr octile::octile(u32 step) :
    axial{step},
    diag{(u32)((double)step * Math::Constants<double>::sqrt2()) + 1},
    shift{(u32)std::countr_zero(step)} // xxx todo assumes power of two chunk size
{
}

constexpr u32 octile::operator()(Vector2i delta) const
{
    const auto d = Vector2ui{Math::abs(delta)};
    const u32 dx = d.x();
    const u32 dy = d.y();
    const u32 mn = dx < dy ? dx : dy;
    const u32 mx = dx > dy ? dx : dy;
    return mx + ((diag - axial) * mn >> shift);
}

constexpr u32 octile::operator()(point cur, point goal) const
{
    return operator()(cur - goal);
}

} // namespace floormat::Search
