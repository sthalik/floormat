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
    struct spot { Vector2i offset; u32 seen_octants = 0; bool done = false, needs_recheck = false; };

    Array<spot> _spots;
    // Indices of the _spots that a critter stood on or may have cut off before _next last went back to 0.
    // They're checked again after every other spot.
    Array<u32> _spot_indices_to_recheck;
    Vector2i _spot_spacing;
    // The next spot, counting _spot_indices_to_recheck after _spots, this pass's counted path_to failures,
    // and whether the pass tries every spot left or only those a ray from the threat doesn't reach.
    u32 _next = 0, _num_failed_dijkstra_calls = 0;
    bool _all = false;
    // The threat's octant seen from the npc.
    u32 _octant = 0;
    // Rays the first pass may still cast, starting at one per spot farther than min_retreat_gain from the npc.
    // That many suffice without a restart, so running out ends the pass early only when the threat changed octant.
    u32 _rays = 0;
    // The first path is_seen_on_path rejected, and its start. A path the first pass found needs
    // is_still_retreat_spot() to test that no ray from the threat reaches its last point.
    path_search_result _exposed_path;
    point _exposed_path_from;
    bool _exposed_path_needs_ray_test = true;
    // The start of the path tick() returned last, which is_still_retreat_spot() reads.
    point _path_from;
    bool _path_needs_ray_test = true;

public:
    // path is found, with is_exposed set when is_seen_on_path rejected it, or not with done false when the
    // update's limits ran out, or with done true once both passes ended.
    struct result { path_search_result path; bool done; bool is_exposed = false; };

    void begin(Vector2i body, u32 threat_octant);
    // One update's work of the retreat, within the max_retreat_*_per_update limits. Each of its two passes ends
    // at max_failed_dijkstra_calls counted path_to failures, the first also when _rays runs out.
    result tick(critter& c, spot_tests& t, const Cover::Script::options& o, point threat);
    // True when `to`, the end of tick()'s last found path, is still over min_retreat_gain px farther from threat
    // than that path's start, and the second pass found it or a ray from threat doesn't reach it.
    bool is_still_retreat_spot(critter& c, spot_tests& t, const Cover::Script::options& o, point to,
                               point threat) const;

    Cover::Script::retreat_progress progress() const;
};

} // namespace floormat::Cover
