#pragma once
#include "raycast.hpp"
#include <Corrade/Containers/Array.h>

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
    Vector2 V, dir, dir_inv_norm;
    Vector2ui size;
    //unsigned short_steps, long_steps;
    float tmin;
};

} // namespace floormat::rc
