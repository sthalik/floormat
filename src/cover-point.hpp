#pragma once
#include "point.hpp"

namespace floormat { class world; }
namespace floormat::Grid { class PoolRegistry; }
namespace floormat::Grid::Cover { struct Params; }

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

cover_point_result find_cover_point(world& w, Grid::PoolRegistry& pools, Grid::Cover::Params params,
                                    const cover_query& q);

} // namespace floormat
