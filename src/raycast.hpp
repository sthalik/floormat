#pragma once
#include "src/object-id.hpp"
#include "src/collision.hpp"
#include "src/point.hpp"

namespace floormat {

class world;

} // namespace floormat

namespace floormat::rc {

struct raycast_diag_s;

struct raycast_result_s
{
    point from, to, collision;
    collision_data collider;
    float time = 0;
    bool has_result : 1 = false,
         success    : 1 = false;
};

[[nodiscard]] raycast_result_s raycast(world& w, point from, point to, object_id self);
[[nodiscard]] raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self);

} // namespace floormat::rc

namespace floormat {

using floormat::rc::raycast;

} // namespace floormat
