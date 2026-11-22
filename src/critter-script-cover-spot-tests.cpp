#include "critter-script-cover-spot-tests.hpp"
#include "critter-script-cover-peek-spot.hpp"
#include "critter-script-cover-search.hpp"
#include "critter.hpp"
#include "grid-registry.hpp"
#include "point.inl"
#include "raycast.hpp"
#include "search-astar.hpp"
#include "search-pred.hpp"
#include "world.hpp"
#include "compat/function-view.hpp"
#include <mg/Functions.h>

namespace floormat::Cover {

using Search::blocks_all_but;

namespace {

constexpr inline float max_seen_from_path_start = 32;
constexpr inline u32 path_body_check_spacing = 16;
constexpr inline u32 threat_lattice_radius = 48;
constexpr inline u32 threat_lattice_spacing = 32;
constexpr inline u32 max_walk_to_hidden_position = (u32)max_seen_from_path_start + path_body_check_spacing;

} // namespace

spot_tests::spot_tests(astar& A, object_id threat_id, Optional<int> gun_offset) :
    _astar{A}, _threat_id{threat_id}, _gun_offset{gun_offset}
{
}

bool spot_tests::has_line_of_sight(critter& c, const Cover::Script::options& o, point from, point to)
{
    counters.num_rays++;
    return Cover::Script::line_of_sight(c.world(), _astar.pool_registry(), o.div_size, from, to, c.id).success;
}

bool spot_tests::is_hidden_at(critter& c, point threat, point pos, Vector2i axis)
{
    counters.num_body_checks++;
    return _finder.is_hidden_at(c.world(), threat, {pos, axis}, Vector2ui(c.bbox_size));
}

ArrayView<const point> spot_tests::reach_positions(critter& c, const Cover::Script::options& o, point threat)
{
    Optional<u32> bbox_side;
    if (const auto obj = c.world().find_object(_threat_id))
        bbox_side = Math::max((u32)obj->bbox_size.x(), (u32)obj->bbox_size.y());
    return _threat_range.border(c.world(), _astar.pool_registry(), threat, {_threat_id, c.id}, bbox_side,
                                o.threat_reach);
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
                            [&](point to) { return has_line_of_sight(c, o, threat, to); });
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

bool spot_tests::is_reachable_without_critters(critter& c, point to)
{
    const auto from = c.position();
    const auto size = Vector2ui(c.bbox_size);
    auto& pool = _astar.pool_registry().pass_pool(
        Grid::Pass::Params{astar::default_div_size, Math::max(size.x(), size.y()), pass_mode::pass});
    counters.num_dijkstra_calls++;
    return _astar.Dijkstra<0, false>(c.world(), pool, from, to, Cover::max_search_path_length(from, to), size,
                                     Search::without_critters).is_found();
}

ArrayView<const point> spot_tests::threat_lattice(critter& c, const Cover::Script::options& o, point threat)
{
    auto& w = c.world();
    Optional<u32> bbox_side;
    if (const auto obj = w.find_object(_threat_id))
        bbox_side = Math::max((u32)obj->bbox_size.x(), (u32)obj->bbox_size.y());
    return _threat_lattice.reachable_lattice_points(w, _astar.pool_registry(), threat, {_threat_id, c.id},
                                                    bbox_side, Math::min(threat_lattice_radius, o.threat_reach),
                                                    threat_lattice_spacing);
}

bool spot_tests::has_hidden_position_nearby(critter& c, const Cover::Script::options& o, point threat)
{
    auto& w = c.world();
    const auto from = c.position();
    if (const auto& r = _nearby_hidden_position;
        r && r->frame_no == w.frame_no() && r->from == from && r->threat == threat)
        return r->found;
    const auto body = Vector2ui(c.bbox_size);
    const auto threat_positions = threat_lattice(c, o, threat);
    const auto side = Math::max(body.x(), body.y());
    const auto spots = _npc_lattice.reachable_lattice_points(w, _astar.pool_registry(), from, {c.id, 0}, side,
                                                             max_walk_to_hidden_position, path_body_check_spacing,
                                                             max_walk_to_hidden_position);
    bool found = false;
    for (auto i = 0u; i < spots.size() && !found; i++)
    {
        const auto q = spots[i];
        found = true;
        for (const auto& t : threat_positions)
        {
            counters.num_path_body_checks++;
            if (!_finder.is_hidden_at(w, t, {q, Cover::Point::axis_toward(t - q)}, body))
            {
                found = false;
                break;
            }
        }
    }
    _nearby_hidden_position = nearby_hidden_position_cache{w.frame_no(), from, threat, found};
    return found;
}

bool spot_tests::is_seen_on_path(critter& c, const Cover::Script::options& o, point threat,
                                 const path_search_result& path)
{
    auto& w = c.world();
    const auto threat_positions = threat_lattice(c, o, threat);
    const auto body = Vector2ui(c.bbox_size);
    bool hidden_before = false, seen_past_start = false;
    const auto is_seen_after_hidden = [&](point pos, float walked) {
        for (const auto& t : threat_positions)
        {
            counters.num_path_body_checks++;
            if (!_finder.is_hidden_at(w, t, {pos, Cover::Point::axis_toward(t - pos)}, body))
            {
                seen_past_start |= !hidden_before && walked > max_seen_from_path_start;
                return hidden_before;
            }
        }
        hidden_before = true;
        return false;
    };
    // The line critter_walk walks.
    const auto smoothings = path.smoothings();
    const auto raw = path.path();
    const auto smoothed = smoothings.size() >= 2;
    const auto count = (u32)(smoothed ? smoothings.size() : raw.size());
    const auto at = [&](u32 i) { return smoothed ? smoothings[i].pt : raw[i]; };
    if (!count)
        return false;
    float walked = 0, next = 0;
    for (auto i = 0u; i + 1 < count; i++)
    {
        const auto a = at(i);
        const auto d = Vector2(at(i + 1) - a);
        const auto len = d.length();
        for (; next < walked + len; next += path_body_check_spacing)
            if (is_seen_after_hidden(a + Vector2i(Math::round(d*((next - walked)/len))), next))
                return true;
        walked += len;
    }
    return is_seen_after_hidden(at(count - 1), walked) ||
           seen_past_start && has_hidden_position_nearby(c, o, threat);
}

Cover::Point::finder& spot_tests::finder() { return _finder; }
Grid::GridRegistry& spot_tests::pool_registry() { return _astar.pool_registry(); }
object_id spot_tests::threat_id() const { return _threat_id; }
Optional<int> spot_tests::gun_offset() const { return _gun_offset; }

} // namespace floormat::Cover
