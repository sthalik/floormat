#pragma once
#include "raycast.hpp"
#include <cr/Array.h>

namespace floormat::detail_rc {

struct bbox
{
    point center;
    Vector2ui size;
};

} // namespace floormat::detail_rc

namespace floormat::rc {

struct raycast_diag_s
{
    Array<floormat::detail_rc::bbox> path{};
    Array<floormat::detail_rc::bbox> queries{};
    Vector2 V, dir, dir_inv_norm;
    float tmin;
};

} // namespace floormat::rc
