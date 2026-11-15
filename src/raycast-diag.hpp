#pragma once
#include "raycast.hpp"
#include <cr/Array.h>

namespace floormat::Raycast::detail {

struct bbox
{
    point center;
    Vector2ui size;
};

} // namespace floormat::Raycast::detail

namespace floormat::Raycast {

struct raycast_diag_s
{
    Array<Raycast::detail::bbox> path{};
    Array<Raycast::detail::bbox> queries{};
    Vector2 V, dir, dir_inv_norm;
    float tmin, radius;
};

} // namespace floormat::Raycast
