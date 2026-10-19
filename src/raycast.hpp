#pragma once
#include "src/object-id.hpp"
#include "src/collision.hpp"
#include "src/point.hpp"
#include "src/search-pred.hpp"

namespace floormat {

class world;

} // namespace floormat

namespace floormat::Grid::Pass {
class Pool;
}

namespace floormat::rc {

struct raycast_diag_s;

struct raycast_result_s
{
    point from, to;
    // On the collider. Radius 0: the first pixel of the line's supercover inside it. Otherwise the
    // point nearest the projectile's center at contact, from + dir * tmin; return tmin if a caller needs it.
    point collision;
    collision_data collider;
    float time = 0;
    bool has_result : 1 = false,
         success    : 1 = false;
};

// A set bit holds for integer positions, while the ray's center reaches up to 1 px past the
// cell's last one, and a rect's pixel squares reach 0.5 px below its min.
constexpr uint32_t min_pool_bbox_size(float radius)
{
    const float size = 2*radius + 3;
    const auto n = (uint32_t)size;
    return n + ((float)n < size);
}

[[nodiscard]] raycast_result_s raycast(world& w, point from, point to, object_id self,
                                       Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred,
                                       float radius = 0);
[[nodiscard]] raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self,
                                                 Grid::Pass::Pool& pass_grid_pool, const Search::pred& pred,
                                                 float radius = 0);

} // namespace floormat::rc

namespace floormat {

using floormat::rc::raycast;

} // namespace floormat
