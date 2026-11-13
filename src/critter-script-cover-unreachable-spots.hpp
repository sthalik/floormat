#pragma once
#include "point.hpp"
#include "search-pred.hpp"
#include <cr/Array.h>

namespace floormat { class world; }

namespace floormat::Cover {

class unreachable_spots
{
public:
    unreachable_spots();

    void clear();
    // True when a body centered on `to` can move in a straight line to a listed spot no more than a tile away
    // on each axis. Then path_to finds no path to `to` either, unless its max_dist is larger.
    bool joins(world& w, point to, Vector2i body, const Search::pred& pred) const;
    // Appends pos unless the list holds it already, as a tile listed again lists its unreachable spots again.
    void add(point pos);
    // Adds pos, which path_to found no path to. reach is how far from search_from its Dijkstra search could
    // reach, and pass_gen is world::pass_gen() after it.
    void add_failed(point pos, u32 reach, u64 pass_gen);
    // Grows the reach by dist, how far search_from moved, to hold every chunk the failed searches could cross.
    void origin_moved(u32 dist);
    // True when a chunk holding a listed spot or one a failed Dijkstra search could cross, or a neighbour of one,
    // has a pass_gen() newer than the last failure. A spot only a critter or a closed door blocked stays skipped.
    bool stale(const world& w, point search_from) const;
    u32 num_failed_dijkstra_calls() const;

private:
    // The search's spots that have no path, or that joins() ties to one.
    Array<point> _spots;
    // How far from search_from the Dijkstra searches that put spots in _spots could reach, and world::pass_gen()
    // once the last of them ended.
    u32 _reach = 0;
    u64 _pass_gen = 0;
    u32 _num_failed_dijkstra_calls = 0;
};

} // namespace floormat::Cover
