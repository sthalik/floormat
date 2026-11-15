#pragma once
#include "point.hpp"
#include "search-result.hpp"
#include <cr/Array.h>

namespace floormat { struct critter; }
namespace floormat::Cover::Script { struct options; struct retreat_progress; }

namespace floormat::Cover {

class spot_tests;

class retreat_finder
{
public:
    // path is found, or not with done false when this update's limits ran out, or with done true once both
    // passes ended.
    struct result { path_search_result path; bool done; };

    void begin(Vector2i body, u32 threat_octant);
    // One update's work of the retreat, within the max_retreat_*_per_update limits. Each of its two passes ends
    // after max_failed_dijkstra_calls spots with no path, the first also when _rays runs out.
    result step(critter& c, spot_tests& t, const Cover::Script::options& o, point threat);
    // True when `to`, the end of step()'s last found path, is still over min_retreat_gain px farther from threat
    // than that path's start, and the second pass found it or a ray from threat doesn't reach it.
    bool keeps(critter& c, spot_tests& t, const Cover::Script::options& o, point to, point threat) const;

    Cover::Script::retreat_progress progress() const;

private:
    struct spot { Vector2i offset; u32 seen_octants = 0; bool done = false; };

    Array<spot> _spots;
    Vector2i _spot_spacing;
    // The next of _spots, this pass's path_to calls that found no path, and whether the pass tries every spot
    // left or only those a ray from the threat doesn't reach.
    u32 _next = 0, _num_failed_dijkstra_calls = 0;
    bool _all = false;
    // The threat's octant seen from the npc.
    u32 _octant = 0;
    // Rays the first pass may still cast, starting at one per spot farther than min_retreat_gain from the npc.
    // That many suffice without a restart, so running out ends the pass early only when the threat changed octant.
    u32 _rays = 0;
};

} // namespace floormat::Cover
