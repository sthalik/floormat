#pragma once
#include "point.hpp"
#include <cr/Array.h>
#include <cr/ArrayView.h>
#include <cr/Optional.h>

namespace floormat { class world; }
namespace floormat::Grid { class GridRegistry; }
namespace floormat::Grid::Pass { class Pool; }

namespace floormat::Cover {

class movement_range
{
    struct cache_key
    {
        point from;
        Optional<u32> bbox_side;
        u32 radius, spacing, max_walk_dist;
        u64 pass_gen;
        bool operator==(const cache_key&) const;
    };

    // Returns the first of the (2n + 1)² cells around the cell of from that dist and passable then hold.
    Vector2i compute_dijkstra_map(world& w, Grid::Pass::Pool& pool, point from, int n, int max_dist);

    // border() or reachable_lattice_points() for the arguments in _key, while world::max_pass_gen() over the
    // chunks it read returns _key->pass_gen. The others are the fill's scratch.
    Array<point> points;
    Array<u16> dist;
    Array<u8> passable;
    Array<u64> heap;
    Optional<cache_key> _key;

public:
    movement_range();

    // from, then the centers of the cells of the walking pass grid for bbox_side that a Dijkstra fill from its
    // cell reaches within max_dist px and that border a cell the fill doesn't reach.
    ArrayView<const point> border(world& w, Grid::GridRegistry& pools, point from, Optional<u32> bbox_side,
                                  u32 max_dist);
    // from, then each other from + (i, j)*spacing within radius px on each axis, when its cell of the walking pass
    // grid for bbox_side lies within max_walk_dist px of walking from from's cell without leaving the square.
    ArrayView<const point> reachable_lattice_points(world& w, Grid::GridRegistry& pools, point from,
                                                    Optional<u32> bbox_side, u32 radius, u32 spacing,
                                                    u32 max_walk_dist = (u32)-1);
};

} // namespace floormat::Cover
