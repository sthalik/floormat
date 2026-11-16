#pragma once
#include "critter-script-cover.hpp"
#include "critter-script-cover-movement-range.hpp"
#include "critter-script-cover-point.hpp"
#include "object-id.hpp"
#include "point.hpp"
#include "search-result.hpp"
#include <cr/ArrayView.h>
#include <cr/Optional.h>

namespace floormat { struct critter; class astar; }
namespace floormat::Grid { class GridRegistry; }

namespace floormat::Cover {

// A path_to call that finds no path costs a Dijkstra search over every node within max_dist, so a search for
// cover or a retreat stops after this many of them.
constexpr inline u32 max_failed_dijkstra_calls = 8;

class spot_tests
{
    struct nearby_hidden_position_cache { u64 frame_no; point from, threat; bool found; };

    ArrayView<const point> threat_lattice(critter& c, const Cover::Script::options& o, point threat);
    bool has_hidden_position_nearby(critter& c, const Cover::Script::options& o, point threat);

    astar& _astar;
    Cover::Point::finder _finder;
    Cover::movement_range _threat_range, _threat_lattice, _npc_lattice;
    Optional<nearby_hidden_position_cache> _nearby_hidden_position;
    object_id _threat_id;
    Optional<int> _gun_offset;

public:
    spot_tests(astar& A, object_id threat_id, Optional<int> gun_offset);

    // Every ray the script casts goes through here, so counters.num_rays counts them all.
    bool sees(critter& c, const Cover::Script::options& o, point from, point to);
    // True when threat sees no pixel of c's body centered on pos, flush against a blocker along axis.
    bool hidden_at(critter& c, point threat, point pos, Vector2i axis);
    // threat, then the centers of the cells of its walking pass grid that a Dijkstra fill from its cell reaches
    // within o.threat_reach px and that border a cell the fill doesn't reach.
    ArrayView<const point> reach_positions(critter& c, const Cover::Script::options& o, point threat);
    // True when no point of reach_positions() for threat sees a pixel of c's body
    // centered on pos, flush along axis.
    bool hidden_from_reach(critter& c, const Cover::Script::options& o, point threat, point pos, Vector2i axis);
    // s moved along its face as far as no point of reach_positions() for threat sees a pixel of c there.
    point cover_end(critter& c, const Cover::Script::options& o, const Cover::Point::spot& s, point threat);
    Optional<Cover::Script::peek> peek_spot(critter& c, const Cover::Script::options& o, point from, Vector2i axis,
                                            point threat);
    // A Dijkstra path from c to `to`, smoothed when string_pull can. Not found when no path within
    // max_search_path_length() leads there.
    path_search_result path_to(critter& c, point to);
    // True when the threat, or a point near it that it can walk to, sees c's body on path after a stretch where it
    // doesn't, or past path's first 32 px when c could walk 48 px to a position none of those points see.
    bool is_seen_on_path(critter& c, const Cover::Script::options& o, point threat, const path_search_result& path);

    Cover::Point::finder& finder();
    Grid::GridRegistry& pool_registry();
    object_id threat_id() const;
    Optional<int> gun_offset() const;

    Cover::Script::counters counters;
};

} // namespace floormat::Cover
