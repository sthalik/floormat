#pragma once
#include "critter-script-cover.hpp"
#include "critter-script-cover-unreachable-spots.hpp"
#include "grid-cover.hpp"
#include "search-result.hpp"
#include "compat/function2.hpp"
#include <array>
#include <cr/Array.h>
#include <cr/Optional.h>

namespace floormat { class world; }
namespace floormat::Grid { class GridRegistry; }

namespace floormat {

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
    // hidden_from_reach's answer, and how many hidden_at calls it made, which step() caps per update.
    struct hidden_result { bool hidden; u32 num_body_checks; };
    // The script's tests of a spot, each with the threat at step()'s `threat`.
    struct calls
    {
        fu2::function_view<hidden_result(point pos, Vector2i axis) const> hidden_from_reach;
        fu2::function_view<bool(point pos, Vector2i axis) const> hidden_at;
        fu2::function_view<point(const Cover::Point::spot& s) const> cover_end;
        fu2::function_view<bool(point end, Vector2i axis) const> has_peek;
        fu2::function_view<path_search_result(point to) const> path_to;
    };

    static constexpr u32 radius = 2*chunk_size_xy;

    void begin(point from, point threat);
    // Keeps the unreachable spots and their failed Dijkstra calls, so a spot with no path from where the npc stood
    // stays skipped as it walks.
    void restart(point from, point threat);
    // One update's work of the search. here is the npc's position, and from is what restart() would be given.
    // A changed pass grid near unreachable spots restarts the search, and a new threat octant resumes its scan.
    result step(world& w, Cover::Point::finder& finder, Grid::GridRegistry& pools, point here, point from,
                point threat, Vector2ui body, const Search::pred& pred, const Cover::Script::options& o,
                u32 max_failed_dijkstra_calls, const calls& f);
    void end();
    // Forgets kept spots that last_resort doesn't allow, which neither a retreat nor step() checks again.
    void drop_last_resort(Cover::Script::options::fallback last_resort);
    void forget_skipped();

    point from() const;
    u32 tile() const;
    u32 tiles_left() const;
    static u32 num_tiles();
    u32 num_find_cover_point_calls() const;
    bool has_last_resort() const;

private:
    struct skipped_spot { Cover::Point::spot spot; u32 octant; };

    Optional<reachable_cover> take_last_resort(point here, const calls& f);

    unreachable_spots _unreachable;
    // The spots of tile _tile, once _tile_listed, of which _tile_spot is next. They were listed with the threat at
    // _tile_threat.
    Array<Cover::Point::spot> _tile_spots;
    // The spots step() skipped, each with its octant, in the tile that octant's scan has reached. A tile listed
    // again skips without testing those it skipped in the current octant.
    Array<skipped_spot> _tile_skipped;
    point _tile_threat;
    // The first spots skipped for having no peek spot and for threat_reach, as options.last_resort keeps.
    Optional<Cover::Point::spot> _peekless, _in_reach;
    u32 _tile = 0, _tile_spot = 0, _octant = 0;
    // Tiles the search may still pass, and 0 once it ended. A restart or a change of octant moves _tile back only
    // while the rest of the scan fits, so a moving threat can't keep the search near the npc.
    u32 _tiles_left = 0;
    std::array<u16, Grid::Cover::octant_count> _octant_tiles{};
    // Where the tiles and the threat's octant are measured from: the npc's position at the last restart to tile 0,
    // or during a retreat walk the walk's end, so that cover back toward the threat comes later.
    point _from;
    u32 _num_find_cover_point_calls = 0;
    bool _tile_listed = false;
};

} // namespace floormat
