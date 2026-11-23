#pragma once
#include "critter-script-cover.hpp"
#include "critter-script-cover-movement-range.hpp"
#include "critter-script-cover-point.hpp"
#include "object-id.hpp"
#include "point.hpp"
#include "search-result.hpp"
#include <cr/ArrayView.h>
#include <cr/Optional.h>

namespace floormat { struct critter; class astar; class world; struct grid_order; }
namespace floormat::Grid { class GridRegistry; }
namespace floormat::Grid::Pass { class Pool; }
namespace floormat::Grid::Object { class Pool; }
namespace floormat::Grid::Cover { class Pool; }

namespace floormat::Cover {

// A Dijkstra search that finds no path may visit every node within max_dist. A cover search's scan, its last
// resort and each retreat pass stop after this many. Only the scan counts reruns without critters.
constexpr inline u32 max_failed_dijkstra_calls = 8;
// A failed Dijkstra search counts toward max_failed_dijkstra_calls only when it added this many nodes or more.
constexpr inline u32 min_failed_dijkstra_added_node_count = 1024;

// The pools of w's registry that the read-only overloads below read: the object pool path_to() reads for a body
// of bbox_side, and the cover pool and the pass pool of line_of_sight() for div_size. Each must exist.
const Grid::Object::Pool& existing_object_pool(const world& w, u32 bbox_side);
const Grid::Cover::Pool& existing_cover_pool(const world& w, u32 div_size);
const Grid::Pass::Pool& existing_shoot_through_pool(const world& w, u32 div_size);

class spot_tests
{
    struct nearby_hidden_position_cache { u64 frame_no; point from, threat; bool found; };

    ArrayView<const point> threat_lattice(const critter& c, const Cover::Script::options& o, point threat,
                                          Array<grid_order>& orders);
    bool has_hidden_position_nearby(const critter& c, const Cover::Script::options& o, point threat,
                                    Array<grid_order>& orders);
    bool is_seen_on_path_impl(const critter& c, const Cover::Script::options& o, point threat,
                              const path_search_result& path, bool is_seen_at_start_counted,
                              Array<grid_order>& orders);

    astar& _astar;
    Cover::Point::finder _finder;
    Cover::movement_range _threat_range;
    // A lattice is the set of points from + (i, j)*spacing.
    // These hold the walkable ones near the threat and near the npc.
    Cover::movement_range _threat_lattice, _npc_lattice;
    Optional<nearby_hidden_position_cache> _nearby_hidden_position;
    object_id _threat_id;
    Optional<int> _gun_offset;

public:
    spot_tests(astar& A, object_id threat_id, Optional<int> gun_offset);

    // Creates in the registry of c's world each pool the read-only overloads below read for c with o, the threat's
    // object pool only once the threat exists.
    void create_pools(critter& c, const Cover::Script::options& o);
    // Appends a grid_order at c's chunk for each pool create_pools() would create, and returns whether it did.
    bool append_orders_for_missing_pools(const critter& c, const Cover::Script::options& o,
                                         Array<grid_order>& orders) const;

    // The overloads taking orders write neither c nor its world, and append a grid_order for each grid they read
    // unbuilt. The others are for the main thread, outside critter_script::on_think.

    // Every ray the script casts goes through here, so counters.num_rays counts them all.
    bool has_line_of_sight(const critter& c, const Cover::Script::options& o, point from, point to,
                           Array<grid_order>& orders);
    bool has_line_of_sight(critter& c, const Cover::Script::options& o, point from, point to);
    // True when threat sees no pixel of c's body centered on pos, flush against a blocker along axis.
    bool is_hidden_at(const critter& c, point threat, point pos, Vector2i axis);
    // True when the threat, at its distance from s.pos in each of the o.octants/2 octants on either side of its
    // own, sees no pixel of c's body there. Counts 2*(o.octants/2) body checks, the most it makes.
    bool is_hidden_from_threat_in_neighbor_octants(const critter& c, const Cover::Script::options& o, point threat,
                                                   const Cover::Point::spot& s);
    // Returns the threat's reach, which is threat and points along the edges of the areas it can walk to within
    // o.threat_reach px and within 3/4 and 1/2 of it.
    ArrayView<const point> reach_positions(const critter& c, const Cover::Script::options& o, point threat,
                                           Array<grid_order>& orders);
    ArrayView<const point> reach_positions(critter& c, const Cover::Script::options& o, point threat);
    // s moved along its face as far as no point of reach_positions() for threat sees a pixel of c there.
    point cover_end(const critter& c, const Cover::Script::options& o, const Cover::Point::spot& s, point threat,
                    Array<grid_order>& orders);
    point cover_end(critter& c, const Cover::Script::options& o, const Cover::Point::spot& s, point threat);
    Optional<Cover::Script::peek> peek_spot(const critter& c, const Cover::Script::options& o, point from,
                                            Vector2i axis, point threat, Array<grid_order>& orders);
    Optional<Cover::Script::peek> peek_spot(critter& c, const Cover::Script::options& o, point from, Vector2i axis,
                                            point threat);
    // A Dijkstra path from c to `to`, smoothed when string_pull can. Not found when no path within
    // max_search_path_length() leads there.
    path_search_result path_to(const critter& c, point to, Array<grid_order>& orders);
    path_search_result path_to(critter& c, point to);
    // True when a Dijkstra search from c that lets every critter through finds a path to `to` within
    // max_search_path_length().
    bool is_reachable_without_critters(const critter& c, point to, Array<grid_order>& orders);
    // The nodes that the last path_to or is_reachable_without_critters call added.
    u32 last_dijkstra_node_count() const;
    // True when the threat, or a point near it that it can walk to, sees c's body on path after a stretch where it
    // doesn't, or past path's first 32 px when c could walk 48 px to a position none of those points see.
    bool is_seen_on_path(const critter& c, const Cover::Script::options& o, point threat,
                         const path_search_result& path, Array<grid_order>& orders);
    // True when the threat, or a point near it that it can walk to, sees c's body anywhere on path.
    bool is_seen_anywhere_on_path(const critter& c, const Cover::Script::options& o, point threat,
                                  const path_search_result& path, Array<grid_order>& orders);

    Cover::Point::finder& finder();
    Grid::GridRegistry& pool_registry();
    object_id threat_id() const;
    Optional<int> gun_offset() const;

    Cover::Script::counters counters;
};

} // namespace floormat::Cover
