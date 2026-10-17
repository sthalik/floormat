#pragma once
#include "point.hpp"

namespace floormat { class world; }
namespace floormat::Grid::Cover { class Pool; }
namespace floormat::Grid::Pass { class Pool; }

namespace floormat {

struct cover_query
{
    // the body's center, as Dijkstra takes it
    point pos;
    Vector2ui body_size;
    point threat;
    uint32_t radius;
};

struct cover_point_result
{
    point pos;
    // toward the threat; pos is flush against a blocker along it
    Vector2i axis;
    uint32_t candidates = 0;
    uint32_t rejected = 0;
    bool found = false;
};

// ray_pool's min_pass decides what blocks the checking rays.
cover_point_result find_cover_point(world& w, Grid::Cover::Pool& cover_pool, Grid::Pass::Pool& ray_pool,
                                    const cover_query& q);

} // namespace floormat
