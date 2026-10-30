#pragma once
#include "point.hpp"
#include "tile-defs.hpp"
#include "cover-point.hpp"
#include "nanosecond.hpp"
#include <cr/Optional.h>

namespace floormat {

struct critter;

struct cover_script_options
{
    enum class stay_phase : uint8_t { alternate, hidden, exposed, };

    // With hidden or exposed, the critter stays in that phase once there: its wait never runs out,
    // and the other phase's runs out in its first standing update. An exposed critter whose cover
    // the threat sees still leaves when exposed_time runs out, for the cover its search found.
    // When the other phase has no peek spot to step out to, the critter tries again after
    // hidden_time. Moving after a threat that sees the critter works as with alternate.
    stay_phase stay = stay_phase::alternate;
    // Looks for cover again on every move of the threat while the critter stands, without first
    // casting rays to see whether the threat sees it. Walks then go on to their end whatever the
    // threat does. For comparing the CPU time of the two.
    bool search_every_move = false;
    // cover_query::octants of the script's searches. With 3, a cover spot must also hide the critter
    // from the threat turned 11.25° either way about it.
    uint8_t octants = 3;
    // cover_query::max_slide of the script's searches. 5 tiles is also their radius, cover_radius.
    uint32_t max_slide = 5*(uint32_t)tile_size_xy;
};

struct cover_script_status
{
    enum class state : uint8_t { seek, to_cover, hidden, to_peek, exposed, retreat, };

    // The cover the critter hides at, peeks from or walks to, when has_cover. While st is exposed
    // and a search found new cover after the threat saw that one, the new cover, which the critter
    // walks to when it stops peeking.
    point cover;
    // The last spot find_peek found, when has_peek. Taking new cover clears has_peek.
    point peek;
    // When has_peek, the ends of a ray from the threat to the gun at peek that the script found
    // clear: gun_seen_from, where the threat stood, and gun, the gun point for that position.
    // find_peek sets them for the spot it finds. check_exposed and check_walk move them when they
    // cast the ray to the gun at peek from where the threat moved to and find it clear. Their
    // comments say when they cast it, and with options.search_every_move they don't run.
    point gun, gun_seen_from;
    // The query of the last find_cover_point call, when has_query, whether it found cover or not.
    cover_query query = {};
    // Since the script started: find_cover_point calls, rays cast from the threat, and Dijkstra
    // searches, those of a retreat included.
    uint32_t searches = 0, rays = 0, paths = 0;
    Ns last_update, max_update, total_update;
    state st = state::seek;
    bool has_cover = false, has_peek = false, has_query = false;
};

// Empty when c runs another script or none.
Optional<cover_script_status> get_cover_script_status(critter& c);

// Where c peeks from a cover spot: pos, its body's center, and gun, its gun point.
struct cover_peek
{
    point pos, gun;
};

// Replaces out with every spot find_cover_point accepts for the status's query, in the order tried.
// When that search found cover and the world hasn't changed since, the first is the spot it found.
// It runs the search again with the script's own pools, div_size and predicate, and the query's
// octants. The script's counters and update times leave it out. out is empty when c runs another
// script or none, or the script hasn't searched yet.
void list_cover_spots(critter& c, Array<cover_spot>& out);

// As list_cover_spots, for q in place of the status's query.
void list_cover_spots(critter& c, const cover_query& q, Array<cover_spot>& out);

// The peek spot the script would step out to from s, with the threat where it stands now. Empty
// when c runs another script or none, the threat is gone, or neither side of s has a peek spot
// within the script's max_peek. A spot the threat already sees gets a peek spot 1 px from it,
// since the slide is at least 1 px. The script's counters and update times leave it out.
Optional<cover_peek> find_cover_peek(critter& c, const cover_spot& s);

// Walks c along a Dijkstra path to s, to hide there for hidden_time and go on as from cover it
// found itself. The threat's position becomes the last check's, as after a search, so the script
// checks whether the threat sees s only once the threat moves. False, with the script unchanged,
// when c runs another script or none, or no path leads to s.
bool send_to_cover(critter& c, const cover_spot& s);

// As send_to_cover, but to find_cover_peek's spot for s, to be exposed there for exposed_time and
// then hide at s. False also when the threat is gone or s has no peek spot.
bool send_to_peek(critter& c, const cover_spot& s);

// From the script's next update on. A changed stay that names the phase other than the one c stands
// in or walks to ends that phase in c's next standing update. Does nothing when c runs another
// script or none.
void set_cover_script_options(critter& c, const cover_script_options& options);

} // namespace floormat
