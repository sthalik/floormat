#pragma once
#include "raycast.hpp"
#include <cr/Array.h>

namespace floormat::detail_Raycast {

struct bbox
{
    point center;
    Vector2ui size;
};

} // namespace floormat::detail_Raycast

namespace floormat::Raycast {

struct raycast_diag_s
{
    Array<detail_Raycast::bbox> path{};
    Array<detail_Raycast::bbox> queries{};
    Vector2 V, dir, dir_inv_norm;
    float tmin, radius;
};

} // namespace floormat::Raycast
