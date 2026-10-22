#pragma once
#include "point.inl"
#include <bit>
#include <mg/Constants.h>
#include <mg/Functions.h>

namespace floormat::Search {

// Step costs of a search stepping `step` px, a power of two. Must match search-astar.cpp directions.
struct octile
{
    uint32_t axial, diag, shift;

    explicit constexpr octile(uint32_t step);
    constexpr uint32_t operator()(Vector2i delta) const;
    constexpr uint32_t operator()(point cur, point goal) const;
};

constexpr octile::octile(uint32_t step) :
    axial{step},
    diag{(uint32_t)((double)step * Math::Constants<double>::sqrt2()) + 1},
    shift{(uint32_t)std::countr_zero(step)} // xxx todo assumes power of two chunk size
{
}

constexpr uint32_t octile::operator()(Vector2i delta) const
{
    const auto d = Vector2ui{Math::abs(delta)};
    const uint32_t dx = d.x();
    const uint32_t dy = d.y();
    const uint32_t mn = dx < dy ? dx : dy;
    const uint32_t mx = dx > dy ? dx : dy;
    return mx + ((diag - axial) * mn >> shift);
}

constexpr uint32_t octile::operator()(point cur, point goal) const
{
    return operator()(cur - goal);
}

} // namespace floormat::Search
