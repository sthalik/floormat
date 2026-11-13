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
    struct cache_key
    {
        point from;
        Optional<u32> bbox_side;
        u32 max_dist;
        u64 pass_gen;
        bool operator==(const cache_key&) const;
    };

    // border() for the arguments in _key, while world::max_pass_gen() over the chunks it read returns
    // _key->pass_gen. The others are its fill's scratch.
    Array<point> points;
    Array<u16> dist;
    Array<u8> passable;
    Array<u64> heap;
    Optional<cache_key> _key;
};

} // namespace floormat::Cover
