#include "critter-script-cover-spot-tests.hpp"
#include "critter-script-cover-peek-spot.hpp"
#include "critter-script-cover-search.hpp"
#include "critter.hpp"
#include "raycast.hpp"
#include "search-astar.hpp"
#include "search-pred.hpp"
#include "world.hpp"
#include "compat/function-view.hpp"
#include <mg/Functions.h>

namespace floormat::Cover {

using Search::blocks_all_but;

spot_tests::spot_tests(astar& A, object_id threat_id, Optional<int> gun_offset) :
    _astar{A}, _threat_id{threat_id}, _gun_offset{gun_offset}
{
}

bool spot_tests::sees(critter& c, const Cover::Script::options& o, point from, point to)
{
    counters.num_rays++;
    return Cover::Script::line_of_sight(c.world(), _astar.pool_registry(), o.div_size, from, to, c.id).success;
}

bool spot_tests::hidden_at(critter& c, point threat, point pos, Vector2i axis)
{
    counters.num_body_checks++;
    return _finder.hidden_at(c.world(), threat, {pos, axis}, Vector2ui(c.bbox_size));
}

ArrayView<const point> spot_tests::reach_positions(critter& c, const Cover::Script::options& o, point threat)
{
    Optional<u32> bbox_side;
    if (const auto obj = c.world().find_object(_threat_id))
        bbox_side = Math::max((u32)obj->bbox_size.x(), (u32)obj->bbox_size.y());
    return _threat_range.border(c.world(), _astar.pool_registry(), threat, bbox_side, o.threat_reach);
}

bool spot_tests::hidden_from_reach(critter& c, const Cover::Script::options& o, point threat, point pos,
                                   Vector2i axis)
{
    for (const auto& t : reach_positions(c, o, threat))
        if (!hidden_at(c, t, pos, axis))
            return false;
    return true;
}

point spot_tests::cover_end(critter& c, const Cover::Script::options& o, const Cover::Point::spot& s, point threat)
{
    const Cover::Point::query q{s.pos, Vector2ui(c.bbox_size), threat, 0, 0, o.max_cover_end_dist};
    const auto from = q.max_cover_end_dist ? reach_positions(c, o, threat).exceptPrefix(1)
                                           : ArrayView<const point>{};
    return _finder.move_to_cover_end(c.world(), q, s.axis, blocks_all_but(c.id), from);
}

Optional<Cover::Script::peek> spot_tests::peek_spot(critter& c, const Cover::Script::options& o, point from,
                                                    Vector2i axis, point threat)
{
    return Cover::peek_spot(c, from, axis, threat, _gun_offset, blocks_all_but(c.id),
                            [&](point to) { return sees(c, o, threat, to); });
}

path_search_result spot_tests::path_to(critter& c, point to)
{
    auto& w = c.world();
    const auto from = c.position();
    const auto body = Vector2ui(c.bbox_size);
    const auto pred = blocks_all_but(c.id);
    const auto max_dist = Cover::max_search_path_length(from, to);
    counters.num_dijkstra_calls++;
    auto path = _astar.Dijkstra<0, false>(w, from, to, max_dist, body, pred);
    if (path.is_found())
        (void)_astar.string_pull(w, path, body, pred);
    return path;
}

Cover::Point::finder& spot_tests::finder() { return _finder; }
Grid::GridRegistry& spot_tests::pool_registry() { return _astar.pool_registry(); }
object_id spot_tests::threat_id() const { return _threat_id; }
Optional<int> spot_tests::gun_offset() const { return _gun_offset; }

} // namespace floormat::Cover
