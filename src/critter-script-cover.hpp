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
    enum class hold_phase : uint8_t { none, hidden, exposed, };

    // hidden or exposed: stay in that phase once there, and its wait never runs out. send_to_cover
    // and send_to_peek set it to the phase they send the critter to.
    hold_phase hold = hold_phase::none;
    // Looks for cover on every move of the threat while the critter stands, without casting rays
    // first, and walks never stop early. For comparing CPU time.
    bool search_every_move = false;
    // cover_query::octants of the script's searches. With 3, a cover spot must also hide the critter
    // from the threat turned 11.25° either way about it.
    uint8_t octants = 3;
    // cover_query::max_cover_end_dist of the script's searches. 5 tiles is also their radius,
    // cover_radius.
    uint32_t max_cover_end_dist = 5*(uint32_t)tile_size_xy;
};

struct cover_script_status
{
    enum class state : uint8_t { seek, to_cover, hidden, to_peek, exposed, retreat, };

    // The cover the critter hides at, peeks from or walks to, when has_cover.
    point cover;
    // The last spot find_peek_pos found, when has_peek. Taking new cover clears has_peek.
    point peek;
    // When has_peek, the ends of the last ray from the threat to the gun at peek that was clear:
    // gun_ray_from, where the threat stood, and gun, the gun point there.
    point gun, gun_ray_from;
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

// Replaces out with every spot find_cover_point accepts for the status's query, in the order tried,
// so the first is the spot it found if the world hasn't changed. Empty when c runs another script
// or none, or the script hasn't searched yet.
void list_cover_spots(critter& c, Array<cover_spot>& out);

// As list_cover_spots, for q in place of the status's query.
void list_cover_spots(critter& c, const cover_query& q, Array<cover_spot>& out);

// The peek spot the script would step out to from s, with the threat where it stands now. Empty
// when c runs another script or none, the threat is gone, or no direction from s has one within
// the script's max_peek_dist.
Optional<cover_peek> find_cover_peek(critter& c, const cover_spot& s);

// Walks c along a Dijkstra path to s, to hide there for hidden_time. An options.hold other than none
// becomes hidden. False, with the script unchanged, when c runs another script or none, or no path
// leads to s.
bool send_to_cover(critter& c, const cover_spot& s);

// As send_to_cover, but to find_cover_peek's spot for s, to be exposed there for exposed_time and
// then hide at s. An options.hold other than none becomes exposed. False also when the threat is
// gone or s has no peek spot.
bool send_to_peek(critter& c, const cover_spot& s);

// From the script's next update on. A changed hold that names the phase other than the one c stands
// in or walks to ends that phase in c's next standing update. Does nothing when c runs another
// script or none.
void set_cover_script_options(critter& c, const cover_script_options& options);

} // namespace floormat
