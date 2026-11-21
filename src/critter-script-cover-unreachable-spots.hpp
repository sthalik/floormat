#pragma once
#include "point.hpp"
#include "search-pred.hpp"
#include "compat/nanosecond.hpp"
#include <cr/Array.h>

namespace floormat { class world; }

namespace floormat::Cover {

// A critter that blocks a path moves no pass_gen(), so is_stale() can't see it leave.
constexpr inline Ns unreachable_spot_retry_time = Minute;

// is_stale() ignores chunk::objects_gen(), since every critter step in range would then clear the list, restart
// the search and rerun the Dijkstra searches that failed.
class unreachable_spots
{
    // Spots that path_to found no path to, or that can_move_straight_to_listed_spot() ties to one.
    Array<point> _spots;
    // No lattice point that a failed Dijkstra search could visit lies farther from search_origin. _pass_gen is
    // world::pass_gen() at the last failure.
    u32 _max_lattice_point_dist = 0;
    u64 _pass_gen = 0;
    Ns _expires_at{};

public:
    unreachable_spots();

    void clear();
    // True when a body centered on `to` can move in a straight line to a listed spot no more than a tile away
    // on each axis. Then path_to finds no path to `to` either, unless its max_dist is larger.
    bool can_move_straight_to_listed_spot(world& w, point to, Vector2i body, const Search::pred& pred) const;
    // Appends pos unless the list holds it already, as a tile listed again lists its unreachable spots again.
    void add(point pos);
    // Adds pos, which path_to found no path to. No lattice point its Dijkstra search could visit lies farther
    // than max_lattice_point_dist from search_origin.
    void add_failed(const world& w, point pos, u32 max_lattice_point_dist);
    // Adds dist, how far search_origin moved, to _max_lattice_point_dist, so the bound holds from the new origin.
    void origin_moved(u32 dist);
    // Removes the spots farther than radius px from center.
    void remove_spots_outside_disc(point center, u32 radius);
    // True when a chunk holding a listed spot or one a failed Dijkstra search could cross, or a neighbour of one,
    // has a pass_gen() newer than the last failure, or once unreachable_spot_retry_time passed since the first.
    bool is_stale(const world& w, point search_origin) const;
};

} // namespace floormat::Cover
