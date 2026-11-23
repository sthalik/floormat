#include "critter-script-cover-spot-tests.hpp"
#include "critter-script-cover-peek-spot.hpp"
#include "critter-script-cover-search.hpp"
#include "critter.hpp"
#include "grid-order.hpp"
#include "grid-registry.hpp"
#include "point.inl"
#include "raycast.hpp"
#include "search-astar.hpp"
#include "search-pred.hpp"
#include "world.hpp"
#include "compat/function-view.hpp"
#include <cr/GrowableArray.h>
#include <mg/Functions.h>

namespace floormat::Cover {

using Search::blocks_all_but;

namespace {

constexpr inline float max_seen_from_path_start = 32;
constexpr inline u32 path_body_check_spacing = 16;
constexpr inline u32 threat_lattice_radius = 48;
constexpr inline u32 threat_lattice_spacing = 32;
constexpr inline u32 max_walk_to_hidden_position = (u32)max_seen_from_path_start + path_body_check_spacing;

u32 bbox_side_of(const object& obj) { return Math::max((u32)obj.bbox_size.x(), (u32)obj.bbox_size.y()); }

Grid::Pass::Params object_pool_params(u32 bbox_side)
{
    return {astar::default_div_size, bbox_side, pass_mode::pass};
}

const Grid::GridRegistry& registry_of(const world& w)
{
    const auto* reg = w.grid_registry();
    fm_assert(reg);
    return *reg;
}

} // namespace

const Grid::Object::Pool& existing_object_pool(const world& w, u32 bbox_side)
{
    const auto* pool = registry_of(w).find_object_pool(object_pool_params(bbox_side));
    fm_assert(pool);
    return *pool;
}

const Grid::Cover::Pool& existing_cover_pool(const world& w, u32 div_size)
{
    const auto* pool = registry_of(w).find_cover_pool(Grid::Cover::Params{div_size});
    fm_assert(pool);
    return *pool;
}

const Grid::Pass::Pool& existing_shoot_through_pool(const world& w, u32 div_size)
{
    const auto* pool = registry_of(w).find_pass_pool(Grid::Pass::Params{div_size, pass_mode::shoot_through});
    fm_assert(pool);
    return *pool;
}

spot_tests::spot_tests(astar& A, object_id threat_id, Optional<int> gun_offset) :
    _astar{A}, _threat_id{threat_id}, _gun_offset{gun_offset}
{
}

void spot_tests::create_pools(critter& c, const Cover::Script::options& o)
{
    auto& reg = _astar.pool_registry();
    (void)reg.object_pool(object_pool_params(bbox_side_of(c)));
    (void)reg.cover_pool(Grid::Cover::Params{o.div_size});
    if (const auto threat = c.world().find_object(_threat_id))
        (void)reg.object_pool(object_pool_params(bbox_side_of(*threat)));
}

bool spot_tests::append_orders_for_missing_pools(const critter& c, const Cover::Script::options& o,
                                                 Array<grid_order>& orders) const
{
    const auto& w = c.world();
    const auto& reg = registry_of(w);
    const auto n = orders.size();
    const auto ch = c.coord.chunk3();
    const auto add_object_pool_order = [&](u32 bbox_side) {
        if (const auto p = object_pool_params(bbox_side); !reg.find_object_pool(p))
            arrayAppend(orders, grid_order{.pass_params = p, .chunk = ch, .kind = grid_order::grid_kind::object});
    };
    const auto npc_side = bbox_side_of(c);
    add_object_pool_order(npc_side);
    if (!reg.find_cover_pool(Grid::Cover::Params{o.div_size}))
        arrayAppend(orders, grid_order{
            .pass_params = Grid::Pass::Params{o.div_size, pass_mode::shoot_through}, .cover_div_size = o.div_size,
            .chunk = ch, .kind = grid_order::grid_kind::cover, .octant = 0,
        });
    if (const auto* threat = w.find_object(_threat_id); threat && bbox_side_of(*threat) != npc_side)
        add_object_pool_order(bbox_side_of(*threat));
    return orders.size() != n;
}

bool spot_tests::has_line_of_sight(const critter& c, const Cover::Script::options& o, point from, point to,
                                   Array<grid_order>& orders)
{
    counters.num_rays++;
    const auto& w = c.world();
    return !Raycast::is_occluded(w, from, to, c.id, existing_shoot_through_pool(w, o.div_size),
                                 Search::without_critters, orders);
}

bool spot_tests::has_line_of_sight(critter& c, const Cover::Script::options& o, point from, point to)
{
    counters.num_rays++;
    auto& pool = Cover::Script::line_of_sight_pass_pool(_astar.pool_registry(), o.div_size);
    return !Raycast::is_occluded(c.world(), from, to, c.id, pool, Search::without_critters);
}

bool spot_tests::is_hidden_at(const critter& c, point threat, point pos, Vector2i axis)
{
    counters.num_body_checks++;
    return _finder.is_hidden_at(c.world(), threat, {pos, axis}, Vector2ui(c.bbox_size));
}

bool spot_tests::is_hidden_from_threat_in_neighbor_octants(const critter& c, const Cover::Script::options& o,
                                                           point threat, const Cover::Point::spot& s)
{
    counters.num_body_checks += 2*(o.octants/2u);
    return _finder.is_hidden_from_threat_in_neighbor_octants(c.world(), threat, s, Vector2ui(c.bbox_size),
                                                             o.octants);
}

ArrayView<const point> spot_tests::reach_positions(const critter& c, const Cover::Script::options& o, point threat,
                                                   Array<grid_order>& orders)
{
    const auto& w = c.world();
    Optional<u32> bbox_side;
    if (const auto* obj = w.find_object(_threat_id))
        bbox_side = bbox_side_of(*obj);
    // Without bbox_side, border() returns threat before reading the pool.
    const auto& pool = existing_object_pool(w, bbox_side ? *bbox_side : bbox_side_of(c)).pass_pool();
    return _threat_range.border(w, pool, threat, bbox_side, o.threat_reach, orders);
}

ArrayView<const point> spot_tests::reach_positions(critter& c, const Cover::Script::options& o, point threat)
{
    Optional<u32> bbox_side;
    if (const auto obj = c.world().find_object(_threat_id))
        bbox_side = bbox_side_of(*obj);
    return _threat_range.border(c.world(), _astar.pool_registry(), threat, bbox_side, o.threat_reach);
}

point spot_tests::cover_end(const critter& c, const Cover::Script::options& o, const Cover::Point::spot& s,
                            point threat, Array<grid_order>& orders)
{
    const Cover::Point::query q{s.pos, Vector2ui(c.bbox_size), threat, 0, 0, o.max_cover_end_dist};
    const auto from = q.max_cover_end_dist ? reach_positions(c, o, threat, orders).exceptPrefix(1)
                                           : ArrayView<const point>{};
    return _finder.move_to_cover_end(c.world(), q, s.axis, blocks_all_but(c.id), from);
}

point spot_tests::cover_end(critter& c, const Cover::Script::options& o, const Cover::Point::spot& s, point threat)
{
    const Cover::Point::query q{s.pos, Vector2ui(c.bbox_size), threat, 0, 0, o.max_cover_end_dist};
    const auto from = q.max_cover_end_dist ? reach_positions(c, o, threat).exceptPrefix(1)
                                           : ArrayView<const point>{};
    return _finder.move_to_cover_end(c.world(), q, s.axis, blocks_all_but(c.id), from);
}

Optional<Cover::Script::peek> spot_tests::peek_spot(const critter& c, const Cover::Script::options& o, point from,
                                                    Vector2i axis, point threat, Array<grid_order>& orders)
{
    return Cover::peek_spot(c, from, axis, threat, _gun_offset, blocks_all_but(c.id),
                            [&](point to) { return has_line_of_sight(c, o, threat, to, orders); });
}

Optional<Cover::Script::peek> spot_tests::peek_spot(critter& c, const Cover::Script::options& o, point from,
                                                    Vector2i axis, point threat)
{
    // Cover::peek_spot() reads chunks as const, which asserts that their passability is current.
    c.world().ensure_passability();
    return Cover::peek_spot(c, from, axis, threat, _gun_offset, blocks_all_but(c.id),
                            [&](point to) { return has_line_of_sight(c, o, threat, to); });
}

path_search_result spot_tests::path_to(const critter& c, point to, Array<grid_order>& orders)
{
    const auto& w = c.world();
    const auto from = c.position();
    const auto body = Vector2ui(c.bbox_size);
    const auto pred = blocks_all_but(c.id);
    const auto max_dist = Cover::max_search_path_length(from, to);
    const auto& objects = existing_object_pool(w, bbox_side_of(c));
    counters.num_dijkstra_calls++;
    auto path = _astar.Dijkstra<0, false>(w, objects, from, to, max_dist, body, pred, orders);
    if (path.is_found())
        (void)_astar.string_pull(w, objects, path, body, pred, orders);
    return path;
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

bool spot_tests::is_reachable_without_critters(const critter& c, point to, Array<grid_order>& orders)
{
    const auto& w = c.world();
    const auto from = c.position();
    const auto size = Vector2ui(c.bbox_size);
    const auto& pool = existing_object_pool(w, bbox_side_of(c)).pass_pool();
    counters.num_dijkstra_calls++;
    return _astar.Dijkstra<0, false>(w, pool, from, to, Cover::max_search_path_length(from, to), size,
                                     Search::without_critters, orders).is_found();
}

u32 spot_tests::last_dijkstra_node_count() const { return _astar.last_node_count(); }

ArrayView<const point> spot_tests::threat_lattice(const critter& c, const Cover::Script::options& o, point threat,
                                                  Array<grid_order>& orders)
{
    const auto& w = c.world();
    Optional<u32> bbox_side;
    if (const auto* obj = w.find_object(_threat_id))
        bbox_side = bbox_side_of(*obj);
    // Without bbox_side, reachable_lattice_points_without_critters() returns threat before reading the pool.
    const auto& objects = existing_object_pool(w, bbox_side ? *bbox_side : bbox_side_of(c));
    return _threat_lattice.reachable_lattice_points_without_critters(
        w, objects, threat, bbox_side, Math::min(threat_lattice_radius, o.threat_reach), threat_lattice_spacing,
        orders);
}

bool spot_tests::has_hidden_position_nearby(const critter& c, const Cover::Script::options& o, point threat,
                                            Array<grid_order>& orders)
{
    const auto& w = c.world();
    const auto from = c.position();
    if (const auto& r = _nearby_hidden_position;
        r && r->frame_no == w.frame_no() && r->from == from && r->threat == threat)
        return r->found;
    const auto body = Vector2ui(c.bbox_size);
    const auto threat_positions = threat_lattice(c, o, threat, orders);
    const auto side = bbox_side_of(c);
    const auto spots = _npc_lattice.reachable_lattice_points(w, existing_object_pool(w, side), from, {c.id, 0}, side,
                                                             max_walk_to_hidden_position, path_body_check_spacing,
                                                             orders, max_walk_to_hidden_position);
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

bool spot_tests::is_seen_on_path(const critter& c, const Cover::Script::options& o, point threat,
                                 const path_search_result& path, Array<grid_order>& orders)
{
    return is_seen_on_path_impl(c, o, threat, path, false, orders);
}

bool spot_tests::is_seen_anywhere_on_path(const critter& c, const Cover::Script::options& o, point threat,
                                          const path_search_result& path, Array<grid_order>& orders)
{
    return is_seen_on_path_impl(c, o, threat, path, true, orders);
}

bool spot_tests::is_seen_on_path_impl(const critter& c, const Cover::Script::options& o, point threat,
                                      const path_search_result& path, bool is_seen_at_start_counted,
                                      Array<grid_order>& orders)
{
    const auto& w = c.world();
    const auto threat_positions = threat_lattice(c, o, threat, orders);
    const auto body = Vector2ui(c.bbox_size);
    bool hidden_before = is_seen_at_start_counted, seen_past_start = false;
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
           seen_past_start && has_hidden_position_nearby(c, o, threat, orders);
}

Cover::Point::finder& spot_tests::finder() { return _finder; }
Grid::GridRegistry& spot_tests::pool_registry() { return _astar.pool_registry(); }
object_id spot_tests::threat_id() const { return _threat_id; }
Optional<int> spot_tests::gun_offset() const { return _gun_offset; }

} // namespace floormat::Cover
