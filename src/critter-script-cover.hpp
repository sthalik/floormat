#pragma once
#include "point.hpp"
#include "cover-point.hpp"
#include "nanosecond.hpp"
#include <cr/Optional.h>

namespace floormat {

struct critter;

struct cover_script_options
{
    // Looks for cover again on every move of the threat while the critter stands, without first
    // casting rays to see whether the threat sees it. Walks then go on to their end whatever the
    // threat does. For comparing the CPU time of the two.
    bool search_every_move = false;
    // cover_query::octants of the script's searches. With 3, a cover spot must also hide the critter
    // from the threat turned 11.25° either way about it.
    uint8_t octants = 3;
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

// Replaces out with every spot find_cover_point accepts for the status's query, in the order tried.
// When that search found cover and the world hasn't changed since, the first is the spot it found.
// It runs the search again with the script's own pools, div_size and predicate, and the query's
// octants. The script's counters and update times leave it out. out is empty when c runs another
// script or none, or the script hasn't searched yet.
void list_cover_spots(critter& c, Array<point>& out);

// From the script's next update on. Does nothing when c runs another script or none.
void set_cover_script_options(critter& c, const cover_script_options& options);

} // namespace floormat
