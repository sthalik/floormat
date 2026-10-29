#pragma once
#include "point.hpp"
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
};

struct cover_script_status
{
    enum class state : uint8_t { seek, to_cover, hidden, to_peek, exposed, retreat, };

    // The cover the critter hides at, peeks from or walks to, when has_cover. While st is exposed
    // and a search found new cover after the threat saw that one, the new cover, which the critter
    // walks to when it stops peeking.
    point cover;
    // The last spot find_peek found, and the gun point there, when has_peek. Taking new cover clears
    // has_peek.
    point peek, gun;
    // Since the script started: find_cover_point calls, rays cast from the threat, and Dijkstra
    // searches, those of a retreat included.
    uint32_t searches = 0, rays = 0, paths = 0;
    Ns last_update, max_update, total_update;
    state st = state::seek;
    bool has_cover = false, has_peek = false;
};

// Empty when c runs another script or none.
Optional<cover_script_status> get_cover_script_status(critter& c);

// From the script's next update on. Does nothing when c runs another script or none.
void set_cover_script_options(critter& c, const cover_script_options& options);

} // namespace floormat
