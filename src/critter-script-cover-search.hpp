#pragma once
#include "critter-script-cover.hpp"
#include "critter-script-cover-unreachable-spots.hpp"
#include "grid-cover.hpp"
#include "search-result.hpp"
#include <array>
#include <cr/Array.h>
#include <cr/Optional.h>

namespace floormat { struct critter; }

namespace floormat::Cover {

class spot_tests;

// path_to's max_dist: twice the distance plus path_slack, as the way around a blocker can be many
// times longer than the straight line.
u32 max_search_path_length(point from, point to);

class cover_search
{
public:
    // end is cover_end() of spot, and path leads there. path is empty when the npc stands on spot or end.
    struct reachable_cover { Cover::Point::spot spot; point end; path_search_result path; };
    // done is true once the search ended, with cover when it found some.
    struct result { Optional<reachable_cover> cover; bool done = false; };

    static constexpr u32 radius = 2*chunk_size_xy;

    void begin(point from, point threat);
    // Keeps the unreachable spots and their failed Dijkstra calls, so a spot with no path from where the npc stood
    // stays skipped as it walks.
    void restart(point from, point threat);
    // One update's work of the search for c. from is what restart() would be given.
    // When the unreachable spots go stale, the search restarts, and a new threat octant resumes its scan.
    result tick(critter& c, spot_tests& t, const Cover::Script::options& o, point from, point threat);
    void end();
    void give_up();
    // Forgets the spots skipped under the old options, and kept spots that last_resort doesn't allow, which
    // neither a retreat nor tick() checks again.
    void options_changed(Cover::Script::options::fallback last_resort);

    bool running() const;
    point from() const;
    Cover::Script::search_progress progress() const;
    bool has_last_resort() const;

private:
    struct skipped_spot { Cover::Point::spot spot; u32 octant; };
    // The first spots skipped for having no peek spot and for threat_reach, as options.last_resort keeps.
    struct fallback_spots
    {
        Optional<Cover::Point::spot> peekless, in_reach;

        // Forgets the spots last_resort doesn't keep.
        void drop(Cover::Script::options::fallback last_resort);
        bool any() const;
    };

    Optional<reachable_cover> take_last_resort(critter& c, spot_tests& t, const Cover::Script::options& o,
                                               point threat);
    Optional<reachable_cover> take_exposed_cover(critter& c, spot_tests& t, const Cover::Script::options& o,
                                                 point threat);

    Optional<reachable_cover> _exposed_cover;
    u32 _exposed_updates_left = 0;
    unreachable_spots _unreachable;
    // The spots of tile _tile while _tile_threat is set, of which _tile_spot is next. They were listed with the
    // threat at _tile_threat.
    Array<Cover::Point::spot> _tile_spots;
    // The spots tick() skipped, each with its octant, in the tile that octant's scan has reached. A tile listed
    // again skips without testing those it skipped in the current octant.
    Array<skipped_spot> _tile_skipped;
    Optional<point> _tile_threat;
    fallback_spots _fallback_spots;
    u32 _tile = 0, _tile_spot = 0, _octant = 0;
    // Tiles the search may still pass, and 0 once it ended. A restart or a change of octant moves _tile back only
    // while the rest of the scan fits, so a moving threat can't keep the search near the npc.
    u32 _tiles_left = 0;
    u32 _num_failed_dijkstra_calls = 0;
    std::array<u16, Grid::Cover::octant_count> _octant_tiles{};
    // Where the tiles and the threat's octant are measured from: the npc's position at the last restart to tile 0,
    // or a retreat walk's end unless is_seen_on_path rejected it, so cover back toward the threat comes later.
    point _from;
};

} // namespace floormat::Cover
