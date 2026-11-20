#pragma once
#include "point.hpp"
#include "search-pred.hpp"
#include "compat/nanosecond.hpp"
#include <cr/Array.h>

namespace floormat { class world; }

namespace floormat::Cover {

// A critter or a closed door that blocks a path moves no pass_gen(), so is_stale() can't see it leave.
constexpr inline Ns unreachable_spot_retry_time = Minute;

class unreachable_spots
{
    // Spots that path_to found no path to, or that joins() ties to one.
    Array<point> _spots;
    // How far from search_origin the Dijkstra searches that put spots in _spots could reach, and world::pass_gen()
    // once the last of them ended.
    u32 _reach = 0;
    u64 _pass_gen = 0;
    Ns _expires_at{};

public:
    unreachable_spots();

    void clear();
    // True when a body centered on `to` can move in a straight line to a listed spot no more than a tile away
    // on each axis. Then path_to finds no path to `to` either, unless its max_dist is larger.
    bool joins(world& w, point to, Vector2i body, const Search::pred& pred) const;
    // Appends pos unless the list holds it already, as a tile listed again lists its unreachable spots again.
    void add(point pos);
    // Adds pos, which path_to found no path to. reach is how far from search_origin its Dijkstra search
    // could reach.
    void add_failed(const world& w, point pos, u32 reach);
    // Grows the reach by dist, how far search_origin moved, to hold every chunk the failed searches could cross.
    void origin_moved(u32 dist);
    // True when a chunk holding a listed spot or one a failed Dijkstra search could cross, or a neighbour of one,
    // has a pass_gen() newer than the last failure, or once unreachable_spot_retry_time passed since the first.
    bool is_stale(const world& w, point search_origin) const;
};

} // namespace floormat::Cover
