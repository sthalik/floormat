#pragma once
#include "object-id.hpp"
#include "point.hpp"
#include <array>
#include <cr/Array.h>
#include <cr/ArrayView.h>
#include <cr/Optional.h>

namespace floormat { class world; }
namespace floormat::Grid { class GridRegistry; }
namespace floormat::Grid::Object { class Pool; }

namespace floormat::Cover {

class movement_range
{
    struct cache_key
    {
        point from;
        std::array<object_id, 2> excluded_blocker_ids;
        Optional<u32> bbox_side;
        u32 radius, spacing, max_walk_dist;
        u64 collider_gen;
        bool operator==(const cache_key&) const;
    };

    // Returns the first of the (2n + 1)² cells around the cell of from that dist and passable then hold. passable
    // leaves out cells blocked by any collider but the critters in excluded_blocker_ids.
    Vector2i compute_dijkstra_map(world& w, Grid::Object::Pool& objects, point from,
                                  std::array<object_id, 2> excluded_blocker_ids, int n, int max_dist);

    // border() or reachable_lattice_points() for _key's arguments, while world::max_collider_gen() over the chunks
    // it read returns _key->collider_gen. The others are the search's scratch arrays.
    Array<point> cached_points;
    Array<u16> dist;
    Array<u8> passable;
    Array<u64> heap;
    Array<bool> entry_blocked;
    Optional<cache_key> _key;

public:
    movement_range();

    // Returns `from` and points along the edge of the area walkable from it within max_dist px.
    ArrayView<const point> border(world& w, Grid::GridRegistry& pools, point from,
                                  std::array<object_id, 2> excluded_blocker_ids, Optional<u32> bbox_side,
                                  u32 max_dist);
    // Returns `from`, then the points `from` + (i, j)*spacing in the square `from` ± radius
    // that are walkable from it within max_walk_dist px without leaving the square.
    ArrayView<const point> reachable_lattice_points(world& w, Grid::GridRegistry& pools, point from,
                                                    std::array<object_id, 2> excluded_blocker_ids,
                                                    Optional<u32> bbox_side, u32 radius, u32 spacing,
                                                    u32 max_walk_dist = (u32)-1);
};

} // namespace floormat::Cover
