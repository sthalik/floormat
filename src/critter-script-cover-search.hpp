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
// Chunk radius around at holding what stops rays to a body within radius px of at, its cover end or peek spot,
// from threat, threat in a neighbor octant, or threat walked up to threat_reach px. Cover grids read 2 more.
u32 search_chunk_radius(point at, point threat, u32 radius, const Cover::Script::options& o);

// is_hidden_at calls that test spots against reach_positions(), each up to 6 microseconds among dense scenery.
// A spot whose test runs out of them is tested further in the next update.
constexpr inline u32 max_search_body_checks_per_update = 80;

enum class visibility_from_reach : u8 { hidden, exposed, unknown, };

// The reach positions that see none of the npc's body at spot, and those that see some, each sorted. Both are
// emptied for another spot or body size, or once a static collider near the spot changed.
struct reach_positions_hidden_from
{
    Cover::Point::spot spot;
    Vector2ub body_size;
    // world::pass_gen() at the last count_reach_positions_seeing().
    u64 pass_gen = 0;
    Array<point> positions, seeing_positions;
    // As in the double buffer pattern, count_reach_positions_seeing() writes the next lists here while reading
    // positions and seeing_positions, then swaps them unless it stopped counting early.
    Array<point> next_positions, next_seeing_positions;

    // Returns how many points of reach see c's body at s, at most max_count + 1, or nothing once the points in
    // neither list need over max_body_checks. max_pass_gen is world::max_pass_gen() around s.
    Optional<u32> count_reach_positions_seeing(critter& c, spot_tests& t, ArrayView<const point> reach,
                                               const Cover::Point::spot& s, u64 max_pass_gen, u32 max_body_checks,
                                               u32 max_count);
    // Whether a point of reach sees c's body at s, as count_reach_positions_seeing() with a max_count of 0.
    visibility_from_reach test_spot_against_reach(critter& c, spot_tests& t, ArrayView<const point> reach,
                                                  const Cover::Point::spot& s, u64 max_pass_gen,
                                                  u32 max_body_checks);
};

class cover_search
{
public:
    // end is cover_end() of spot, and path leads there, empty when the npc stands on spot or end. peek_spot is
    // the peek spot from end for the threat that tick() was given, when that tick() looked for one.
    struct reachable_cover
    {
        Cover::Point::spot spot;
        point end;
        path_search_result path;
        Optional<Cover::Script::peek> peek_spot;
        // False for the in_reach fallback, which a point of reach_positions() saw.
        bool is_hidden_from_reach = true;
    };
    // done is true once the search ended, with cover when it found some.
    struct result { Optional<reachable_cover> cover; bool done = false; };

    static constexpr u32 radius = 2*chunk_size_xy;

    cover_search();
    // Without is_fallback_allowed, the search ends with cover hidden from reach that the threat sees nowhere on
    // its path, or with none.
    void begin(point from, point threat, bool is_fallback_allowed = true);
    // Keeps the counts of failed Dijkstra calls and the spots no path reaches with critters let through, so those
    // stay skipped as the npc walks. A restart from tile 0 tries again the spots only critters may have blocked.
    void restart(point from, point threat);
    // One update's work of the search for c, with from as restart() takes it. Stale unreachable spots, or walls,
    // holes or static scenery changing nearby, restart the search. A new threat octant resumes its scan.
    result tick(critter& c, spot_tests& t, const Cover::Script::options& o, point from, point threat,
                u32 max_octant_fills);
    void end();
    void give_up();

    bool is_running() const;
    point from() const;
    Cover::Script::search_progress progress() const;
    bool has_last_resort() const;

private:
    struct skipped_spot { Cover::Point::spot spot; u32 octant; };
    // A spot and its cover_end(), which path_to found no path to.
    struct failed_dijkstra_search { point spot_pos, end; bool is_fallback_spot = false; };
    // Spots skipped for having no peek spot, and of those skipped for threat_reach the ones the fewest reach
    // positions see, as options.last_resort keeps.
    struct fallback_spots
    {
        // A spot skipped for threat_reach, and how many reach positions saw the npc's body there.
        struct in_reach_spot { Cover::Point::spot spot; u32 num_reach_positions_seeing; };

        Array<Cover::Point::spot> peekless_spots;
        // Sorted by num_reach_positions_seeing, fewest first.
        Array<in_reach_spot> in_reach_spots;

        bool has_cover_spot() const;
        void clear();
        // Inserts s after the spots no more reach positions see, and drops the last spot of a full list. More
        // reach positions must see that spot than s.
        void add_in_reach_spot(const Cover::Point::spot& s, u32 num_reach_positions_seeing);
    };

    // _reach_positions_not_seeing_spot's test of s against reach_positions() for threat. max_pass_gen is
    // world::max_pass_gen() over the chunks the search reads.
    visibility_from_reach test_spot_against_reach(critter& c, spot_tests& t, const Cover::Script::options& o,
                                                  point threat, const Cover::Point::spot& s, u64 max_pass_gen,
                                                  u32 max_body_checks);
    Optional<reachable_cover> take_last_resort(critter& c, spot_tests& t, const Cover::Script::options& o,
                                               point threat, u64 max_pass_gen, u32 max_body_checks);
    Optional<reachable_cover> take_exposed_cover(critter& c, spot_tests& t, const Cover::Script::options& o,
                                                 point threat, u64 max_pass_gen, u32 max_body_checks);

    Optional<reachable_cover> _exposed_cover;
    reach_positions_hidden_from _reach_positions_not_seeing_spot;
    u32 _exposed_updates_left = 0;
    // Spots that no path reaches with other critters let through, or from which the npc can move straight to one.
    unreachable_spots _unreachable;
    // Spots that path_to found no path to with other critters blocking, or from which the npc can move straight
    // to one. Skipped until the scan starts again from tile 0.
    unreachable_spots _unreachable_until_rescan;
    // Runs in the next tick() with critters let through, and adds the spot to _unreachable when no path is found.
    // The search doesn't end while it waits.
    Optional<failed_dijkstra_search> _dijkstra_search_to_rerun;
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
    // Unlike _num_failed_dijkstra_calls, it doesn't count reruns.
    u32 _num_failed_dijkstra_calls_for_fallback = 0;
    bool _is_fallback_allowed = true;
    // world::pass_gen() at the last tick(), or 0 before the first after begin().
    u64 _pass_gen_at_last_tick = 0;
    std::array<u16, Grid::Cover::octant_count> _octant_tiles{};
    // Where the tiles and the threat's octant are measured from: the npc's position at the last restart to tile 0,
    // or a retreat walk's end unless is_seen_on_path rejected it, so cover back toward the threat comes later.
    point _from;
};

} // namespace floormat::Cover
