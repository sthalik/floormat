#pragma once
#include "object-id.hpp"
#include "collision.hpp"
#include "point.hpp"
#include "search-pred.hpp"
#include <mg/Range.h>

namespace floormat {

class world;

} // namespace floormat

namespace floormat::Grid::Pass { class Pool; }
namespace floormat::Grid::Object { class Pool; }

namespace floormat::Raycast {

struct raycast_diag_s;

struct raycast_result_s
{
    point from, to;
    // With radius 0 this is the first pixel of the line's supercover inside the collider. Otherwise it is the
    // collider's point nearest the projectile's center at contact, from + dir * tmin.
    point collision;
    collision_data collider;
    float time = 0;
    bool has_result : 1 = false,
         success    : 1 = false;
};

// Returns ceil(2*radius + 3), the smallest pass grid bbox_size raycast() accepts for this radius. A set bit holds
// only for integer positions, while the ray's center reaches up to 1 px past the cell's last one.
constexpr u32 min_pool_bbox_size(float radius)
{
    const float size = 2*radius + 3;
    const auto n = (u32)size;
    return n + ((float)n < size);
}

// With only a pass pool, a ray misses critters in cells that no static collider blocks.
[[nodiscard]] raycast_result_s raycast(world& w, point from, point to, object_id self,
                                       Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred, float radius = 0);
[[nodiscard]] raycast_result_s raycast(world& w, point from, point to, object_id self,
                                       Grid::Object::Pool& object_pool, Search::pred const& pred, float radius = 0);
[[nodiscard]] raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self,
                                                 Grid::Pass::Pool& pass_grid_pool,
                                                 const Search::pred& pred, float radius = 0);
[[nodiscard]] raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self,
                                                 Grid::Object::Pool& object_pool,
                                                 const Search::pred& pred, float radius = 0);

} // namespace floormat::Raycast

namespace floormat {

using floormat::Raycast::raycast;

} // namespace floormat
