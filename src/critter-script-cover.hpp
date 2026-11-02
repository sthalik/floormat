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
    // The spots a search keeps of those it skips. With each value, it also keeps what the values before keep.
    enum class fallback : uint8_t {
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
    // cover_query::octants of the script's searches. With 3, a cover spot must also hide the critter
    // from the threat turned 11.25° either way about it.
    uint8_t octants = 3;
    // How far in px the critter's cover may lie from its spot, along their face.
    uint32_t max_cover_end_dist = 5*(uint32_t)tile_size_xy;
    // The div_size of the grids the script searches and casts rays in, which make_cover_script sets.
    // Options built from {} set it back to 8.
    uint32_t div_size = 8;
    // In px. Spots and their cover must also hide the critter from where the threat can walk within this
    // distance. 0 tests only where it stands.
    uint32_t threat_reach = 2*(uint32_t)tile_size_xy;
    // The search skips spots whose cover has no peek spot.
    bool needs_peek = true;
    // When the search ends with no other cover, the critter hides at a kept spot. If one is kept while the threat
    // sees the critter, the critter retreats while the search goes on, once per search.
    fallback last_resort = fallback::in_reach;

    // The script's searches' cover_query::max_cover_end_dist. 0 with threat_reach, as their move ignores it.
    uint32_t listed_cover_end_dist() const { return threat_reach ? 0 : max_cover_end_dist; }
};

struct cover_script_status
{
    // search: standing while it looks for cover, a few tiles per update. find_retreat: standing while
    // it looks for a path to a spot farther from the threat, a few paths per update.
    enum class state : uint8_t { seek, to_cover, hidden, to_peek, exposed, retreat, search, find_retreat, };

    // The cover the critter hides at, peeks from or walks to, when has_cover. canonical is the spot found for
    // it, from where cover lies along their face as far as the threat alone, without its turns, sees no pixel.
    point cover, canonical;
    // The last spot find_peek_pos found, when has_peek. Taking new cover clears has_peek.
    point peek;
    // When has_peek, the ends of the last ray from the threat to the gun at peek that was clear:
    // gun_ray_from, where the threat stood, and gun, the gun point there.
    point gun, gun_ray_from;
    // When has_query, the last finished search's query, found or not. Its radius is cut to a tile
    // past the spot found.
    cover_query query = {};
    // Since the script started: finished searches for cover, rays cast from the threat, tests of the body at
    // a spot against the threat, and Dijkstra searches, those of a retreat included.
    uint32_t searches = 0, rays = 0, body_checks = 0, paths = 0;
    // While a search runs, in state::search or during a retreat, the number of tiles tried out of search_tiles.
    uint32_t search_tile = 0, search_tiles = 0;
    // In state::find_retreat, the next of retreat_spots spots, and this pass's spots with no path. The
    // second pass, retreat_all, also tries spots a ray from the threat reaches.
    uint32_t retreat_spot = 0, retreat_spots = 0, retreat_failed = 0;
    bool retreat_all = false;
    // Since the script started, the times find_retreat went back to its first spot because the threat
    // moved to another octant of the cover grid, seen from the critter, or a retreat walk was blocked or flanked.
    uint32_t retreat_restarts = 0;
    // Since the script started, the searches' find_cover_point calls, each for one tile. A tile gets
    // another call when the threat moved since its list was made, or when the call ran out of fills.
    uint32_t tile_lists = 0;
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

// As list_cover_spots, for q in place of the status's query, returning find_cover_point's result.
// Empty when c runs another script or none.
cover_point_result list_cover_spots(critter& c, const cover_query& q, Array<cover_spot>& out);

// The peek spot the script steps out to after send_to_cover(c, s), with the threat where it stands now. Empty
// when c runs another script or none, the threat is gone, or no direction has one within max_peek_dist.
Optional<cover_peek> find_cover_peek(critter& c, const cover_spot& s);

// Walks c along a Dijkstra path to hide for hidden_time at s moved to its cover's end, as cover is from canonical.
// An options.hold other than none becomes hidden. False, changing nothing, with another script, none, or no path.
bool send_to_cover(critter& c, const cover_spot& s);

// As send_to_cover, but to find_cover_peek's spot for s, to be exposed there for exposed_time and
// then hide. An options.hold other than none becomes exposed. False also when the threat is
// gone or s has no peek spot.
bool send_to_peek(critter& c, const cover_spot& s);

// From the script's next update on. A changed hold that names the phase other than the one c stands
// in or walks to ends that phase in c's next standing update. Does nothing when c runs another
// script or none.
void set_cover_script_options(critter& c, const cover_script_options& options);

} // namespace floormat
