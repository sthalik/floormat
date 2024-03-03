#pragma once
#include "point.hpp"
#include <mg/Functions.h>

namespace floormat {

constexpr uint32_t point::distance(point a, point b)
{
    Vector2i dist;
    dist += (a.coord() - b.coord())*iTILE_SIZE2;
    dist += Vector2i(a.offset()) - Vector2i(b.offset());
    return (uint32_t)Math::ceil(Math::sqrt(Vector2(dist).dot()));
}

constexpr uint32_t point::distance_l2(point a, point b)
{
    Vector2i dist;
    dist += (a.coord() - b.coord())*iTILE_SIZE2;
    dist += Vector2i(a.offset()) - Vector2i(b.offset());
    return (uint32_t)Math::abs(dist).sum();
}

} // namespace floormat
