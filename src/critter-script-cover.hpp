#pragma once
#include "compat/borrowed-ptr-fwd.hpp"
#include "compat/nanosecond.hpp"
#include "object-id.hpp"
#include "point.hpp"
#include "tile-defs.hpp"
#include "cover-point.hpp"
#include <cr/Optional.h>

namespace floormat { struct critter; struct object; class world; }

namespace floormat::Cover::Script {

struct options
{
    enum class hold_phase : u8 { none, hidden, exposed, };
    // The spots a search keeps of those it skips. With each value, it also keeps what the values before keep.
    enum class fallback : u8 {
        none,
        // The first spot whose cover has no peek spot.
        no_peek,
        // The first spot the threat would see after walking threat_reach px. The critter hides there only when no
        // no_peek spot still hides it.
        in_reach,
    };

    // hidden or exposed: stay in that phase once there, and its wait never runs out. send_to_cover
    // and send_to_peek set it to the phase they send the critter to.
    hold_phase hold = hold_phase::none;
    // Looks for cover on every move of the threat while the critter stands, without casting rays
    // first, and walks never stop early. For comparing CPU time.
    bool search_every_move = false;
    // Cover::Point::query::octants of the script's searches. With 3, a cover spot must also hide the critter
    // from the threat turned 11.25° either way about it.
    u8 octants = 3;
    // How far in px the critter's cover may lie from its spot, along their face.
    u32 max_cover_end_dist = 5*(u32)tile_size_xy;
    // The div_size of the grids the script searches and casts rays in, which make_cover_script sets.
    // options built from {} set it back to 8.
    u32 div_size = 8;
    // In px. Spots and their cover must also hide the critter from where the threat can walk within this
    // distance. 0 tests only where it stands.
    u32 threat_reach = 2*(u32)tile_size_xy;
    // The search skips spots whose cover has no peek spot.
    bool needs_peek = true;
    // When the search ends with no other cover, the critter hides at a kept spot. If one is kept while the threat
    // sees the critter, the critter retreats while the search goes on, once per search.
    fallback last_resort = fallback::in_reach;

    // The searches' max_cover_end_dist. 0 with threat_reach, as their move ignores it.
    u32 listed_cover_end_dist() const { return threat_reach ? 0 : max_cover_end_dist; }
};

struct status
{
    // search: standing while it looks for cover, a few tiles per update. find_retreat: standing while
    // it looks for a path to a spot farther from the threat, a few Dijkstra searches per update.
    enum class state : u8 { seek, to_cover, hidden, to_peek, exposed, retreat, search, find_retreat, };

    // The cover the critter hides at, peeks from or walks to, when has_cover. canonical is the spot found for
    // it, from where cover lies along their face as far as the threat alone, without its turns, sees no pixel.
    point cover, canonical;
    // canonical's axis, toward the threat, when has_cover.
    Vector2i axis;
    // The last spot find_peek_pos found, when has_peek. Taking new cover clears has_peek.
    point peek;
    // When has_peek, the ends of the last ray from the threat to the gun at peek that was clear:
    // gun_ray_from, where the threat stood, and gun, the gun point there.
    point gun, gun_ray_from;
    // When has_query, the last finished search's query, found or not. Its radius is cut to a tile
    // past the spot found.
    Cover::Point::query query = {};
    // Since the script started: finished searches for cover, rays cast from the threat, tests of the body at
    // a spot against the threat, and Dijkstra searches, those of a retreat included.
    u32 num_searches = 0, num_rays = 0, num_body_checks = 0, num_dijkstra_calls = 0;
    // The tiles a search may still pass, 0 when none runs, and the number it tried out of num_search_tiles.
    u32 search_tiles_left = 0, search_tile = 0, num_search_tiles = 0;
    // In state::find_retreat, the next of num_retreat_spots spots, and this pass's path_to calls that found no
    // path. The second pass, retreat_all, also tries spots a ray from the threat reaches.
    u32 retreat_spot = 0, num_retreat_spots = 0, num_failed_retreat_dijkstra_calls = 0;
    bool retreat_all = false;
    // Since the script started, the times find_retreat went back to its first spot because the threat
    // moved to another octant of the cover grid, seen from the critter, or a retreat walk was blocked or flanked.
    u32 num_retreat_restarts = 0;
    // Since the script started, the searches' Cover::Point::finder::find calls, each for one tile. A tile gets
    // another call when the threat moved since its list was made, or when the call ran out of fills.
    u32 num_find_cover_point_calls = 0;
    Ns last_update, max_update, total_update;
    state st = state::seek;
    bool has_cover = false, has_peek = false, has_query = false;
};

// Where c peeks from a cover spot: pos, its body's center, and gun, its gun point.
struct peek
{
    point pos, gun;
};

// Empty when c runs another script or none.
Optional<status> get_status(critter& c);

// Replaces out with every spot accepted for the last search's query, in search order.
// Empty when c runs another script or none, or the script hasn't searched yet.
void list_spots(critter& c, Array<Cover::Point::spot>& out);

// As list_spots, for q in place of the status's query, returning Cover::Point::finder::find's result.
// Empty when c runs another script or none.
Cover::Point::result list_spots(critter& c, const Cover::Point::query& q, Array<Cover::Point::spot>& out);

// The peek spot the script steps out to after send_to_cover(c, s), with the threat where it stands now. Empty
// when c runs another script or none, no threat is on c's floor, or no direction has one within max_peek_dist.
Optional<peek> find_peek(critter& c, const Cover::Point::spot& s);

// Walks c along a Dijkstra path to hide for hidden_time at s moved to its cover's end, as cover is from canonical.
// A hold becomes hidden. False, changing nothing, with another script, none, no threat on c's floor or no path.
bool send_to_cover(critter& c, const Cover::Point::spot& s);

// Sends c to find_peek's spot for s, stays exposed for exposed_time, then hides. A hold becomes exposed.
// False, changing nothing, with another script, none, no threat on c's floor, no path or no peek spot.
bool send_to_peek(critter& c, const Cover::Point::spot& s);

// From the script's next update on. A changed hold that names the phase other than the one c stands
// in or walks to ends that phase in c's next standing update. Does nothing when c runs another
// script or none.
void set_options(critter& c, const options& options);

// Object id of w, or empty when it's gone or its chunk3().z isn't z.
bptr<object> find_object_at_z(world& w, object_id id, i8 z);

} // namespace floormat::Cover::Script
