#pragma once
#include "point.hpp"
#include <cr/Array.h>
#include <cr/ArrayView.h>
#include <cr/Optional.h>

namespace floormat { class world; }
namespace floormat::Grid { class GridRegistry; }

namespace floormat::Cover {

class movement_range
{
public:
    movement_range();

    // from, then the centers of the cells of the walking pass grid for bbox_side that a Dijkstra fill from its
    // cell reaches within max_dist px and that border a cell the fill doesn't reach.
    ArrayView<const point> border(world& w, Grid::GridRegistry& pools, point from, Optional<u32> bbox_side,
                                  u32 max_dist);

private:
    // border() for the arguments saved in last_from, last_bbox_side and last_max_dist, while world::max_pass_gen()
    // over the chunks it read returns last_pass_gen. The others are its fill's scratch.
    Array<point> points;
    Array<u16> dist;
    Array<u8> passable;
    Array<u64> heap;
    point last_from;
    u64 last_pass_gen = 0;
    Optional<u32> last_bbox_side;
    u32 last_max_dist = 0;
};

} // namespace floormat::Cover
