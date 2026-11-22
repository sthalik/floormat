#include "critter-script-cover-retreat-finder.hpp"
#include "critter-script-cover.hpp"
#include "critter-script-cover-point.hpp"
#include "critter-script-cover-spot-tests.hpp"
#include "critter.hpp"
#include "grid-cover.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "search.hpp"
#include "search-pred.hpp"
#include "tile-defs.hpp"
#include "world.hpp"
#include "compat/assert.hpp"
#include "compat/function-view.hpp"
#include <algorithm>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat::Cover {

namespace {

// A retreat ends at a spot within retreat_radius of the npc that is over min_retreat_gain px farther from the
// threat. As the gain can't exceed the distance, that spot is over min_retreat_gain px from the npc too.
constexpr u32 retreat_radius = 5*(u32)tile_size_xy;
constexpr int min_retreat_gain = 3*tile_size_xy;
constexpr u32 max_retreat_dijkstra_calls_per_update = 4;
// is_seen_on_path's is_hidden_at calls that count as one Dijkstra search toward that cap, after an update's first.
// A search that found a path took up to 0.71 ms among dense scenery, a call 0.33 to 0.82 microseconds.
constexpr u32 hidden_at_calls_per_dijkstra_call = 710;
// has_line_of_sight() calls, each one ray from the threat to a spot's center.
constexpr u32 max_retreat_rays_per_update = 256;
// Search::is_passable calls for the npc's body at a spot, each an R-tree search.
constexpr u32 max_retreat_is_passable_calls_per_update = 512;

static_assert(Grid::Cover::octant_count <= 32);

} // namespace

void retreat_finder::begin(Vector2i body, u32 threat_octant)
{
    const auto spacing = Math::max(body/2, Vector2i{1});
    if (spacing != _spot_spacing)
    {
        _spot_spacing = spacing;
        const auto n = Vector2i{(int)retreat_radius} / spacing;
        const auto count = (u32)((2*n.x() + 1)*(2*n.y() + 1));
        if (arrayCapacity(_spots) < count)
            arrayReserve(_spots, count);
        arrayClear(_spots);
        for (auto y = -n.y(); y <= n.y(); y++)
            for (auto x = -n.x(); x <= n.x(); x++)
                if (const auto off = Vector2i{x, y} * spacing; off.dot() <= (int)(retreat_radius*retreat_radius))
                    arrayAppend(_spots, spot{off});
        // std::sort leaves the order of equal elements to the standard library, so the offset breaks
        // the tie and every library picks the same spot.
        std::sort(_spots.begin(), _spots.end(), [](const spot& a, const spot& b) {
            if (const auto da = a.offset.dot(), db = b.offset.dot(); da != db)
                return da < db;
            return a.offset.y() != b.offset.y() ? a.offset.y() < b.offset.y() : a.offset.x() < b.offset.x();
        });
    }
    _rays = 0;
    for (auto& s : _spots)
    {
        s.done = s.needs_recheck = false;
        s.seen_octants = 0;
        _rays += s.offset.dot() > min_retreat_gain*min_retreat_gain;
    }
    arrayReserve(_spot_indices_to_recheck, 16);
    arrayClear(_spot_indices_to_recheck);
    _next = 0;
    _num_failed_dijkstra_calls = 0;
    _all = false;
    _octant = threat_octant;
    _exposed_path = {};
}

retreat_finder::result retreat_finder::tick(critter& c, spot_tests& t, const Cover::Script::options& o,
                                            point threat)
{
    auto& w = c.world();
    const auto from = c.position();
    const auto body = Vector2i(c.bbox_size);
    const auto restart_from_first_spot = [&] {
        _next = 0;
        arrayClear(_spot_indices_to_recheck);
        for (auto i = 0u; i < _spots.size(); i++)
            if (_spots[i].needs_recheck)
                arrayAppend(_spot_indices_to_recheck, i);
    };
    if (const auto k = Cover::Point::nearest_octant(Vector2(threat - from)); k != _octant)
    {
        // Spots the threat saw from its old octant may be hidden now. The pass and its count of path_to failures
        // stay, so a moving threat can't cause more than 2*max_failed_dijkstra_calls counted ones.
        _octant = k;
        restart_from_first_spot();
        t.counters.num_retreat_restarts++;
    }
    const auto octant_bit = 1u << _octant;
    const auto here = Math::sqrt((double)point::distance_squared(from, threat));
    auto dijkstra_left = max_retreat_dijkstra_calls_per_update, rays_left = max_retreat_rays_per_update;
    auto passable_left = max_retreat_is_passable_calls_per_update;
    const auto path_body_checks_before_step = t.counters.num_path_body_checks;
    const auto take_exposed_path = [&]() -> result {
        _path_from = _exposed_path_from;
        _path_needs_ray_test = _exposed_path_needs_ray_test;
        auto p = move(_exposed_path);
        _exposed_path = {};
        return {move(p), false, true};
    };
    const auto on_limit_reached = [&]() -> result {
        if (_exposed_path.is_found())
            return take_exposed_path();
        else
            return {};
    };
    for (;;)
    {
        const auto num_spots = (u32)_spots.size();
        if (_next == num_spots + _spot_indices_to_recheck.size() ||
            _num_failed_dijkstra_calls >= max_failed_dijkstra_calls || !_all && !_rays)
        {
            if (_all && _exposed_path.is_found())
                return take_exposed_path();
            if (_all)
                return {{}, true};
            _all = true;
            restart_from_first_spot();
            _num_failed_dijkstra_calls = 0;
            continue;
        }
        const auto is_recheck = _next >= num_spots;
        auto& s = _spots[is_recheck ? _spot_indices_to_recheck[_next - num_spots] : _next];
        const auto to = from + s.offset;
        if (s.done && !is_recheck || !_all && (s.seen_octants & octant_bit))
        {
            _next++;
            continue;
        }
        if (Math::sqrt((double)point::distance_squared(to, threat)) - here <= min_retreat_gain)
        {
            _next++;
            continue;
        }
        // Dijkstra finds no path into a chunk that doesn't exist.
        if (!w.at(to.chunk3()))
        {
            s.done = true;
            _next++;
            continue;
        }
        // The limits come first, so no is_passable result is thrown away when one runs out. A spot takes up to
        // 2 Dijkstra searches, the second when path_to finds no path.
        if (!passable_left || dijkstra_left < 2 || !_all && !rays_left)
            return on_limit_reached();
        if (dijkstra_left < max_retreat_dijkstra_calls_per_update &&
            t.counters.num_path_body_checks - path_body_checks_before_step >
                (dijkstra_left - 2)*hidden_at_calls_per_dijkstra_call)
            return on_limit_reached();
        passable_left--;
        t.counters.num_retreat_is_passable_calls++;
        s.done = s.needs_recheck = false;
        bool is_blocked_by_critter = false;
        const auto pred = [&](const chunk& ch, collision_data x) {
            if (x.id == c.id)
                return path_search_continue::pass;
            else if (Search::is_dynamic_object(ch, x))
            {
                is_blocked_by_critter = true;
                return path_search_continue::pass;
            }
            else
                return path_search_continue::blocked;
        };
        const auto lo = Vector2(to.intra().center_shifted() - body/2);
        if (!Search::is_passable(w, to.chunk3(), Range2D::fromSize(lo, Vector2(body)), pred))
        {
            s.done = true;
            _next++;
            continue;
        }
        if (is_blocked_by_critter)
        {
            s.done = s.needs_recheck = true;
            _next++;
            continue;
        }
        if (!_all)
        {
            rays_left--;
            _rays--;
            if (t.has_line_of_sight(c, o, threat, to))
            {
                s.seen_octants |= octant_bit;
                _next++;
                continue;
            }
        }
        dijkstra_left--;
        s.done = true;
        _next++;
        // TODO: a Dijkstra search mode that adds the threat seeing the npc to a path's cost could use another
        // cost here than for paths to cover.
        if (auto path = t.path_to(c, to); path.is_found())
        {
            if (!t.is_seen_on_path(c, o, threat, path))
            {
                _path_from = from;
                _path_needs_ray_test = !_all;
                return {move(path), false};
            }
            if (!_exposed_path.is_found())
            {
                _exposed_path = move(path);
                _exposed_path_from = from;
                _exposed_path_needs_ray_test = !_all;
            }
            continue;
        }
        // A path_to that another critter blocks often fails before adding enough nodes to count, so a pass
        // where critters block every path can try every spot, two per update.
        if (t.last_dijkstra_node_count() >= min_failed_dijkstra_added_node_count)
            _num_failed_dijkstra_calls++;
        dijkstra_left--;
        // path_to lets no other critter through, so its failure doesn't say whether a critter cut the spot off.
        s.needs_recheck = t.is_reachable_without_critters(c, to);
    }
}

bool retreat_finder::is_still_retreat_spot(critter& c, spot_tests& t, const Cover::Script::options& o,
                                           point to, point threat) const
{
    fm_debug_assert(_next > 0);
    const auto dist = [&](point p) { return Math::sqrt((double)point::distance_squared(p, threat)); };
    return dist(to) - dist(_path_from) > min_retreat_gain && (!_path_needs_ray_test || !t.has_line_of_sight(c, o, threat, to));
}

Cover::Script::retreat_progress retreat_finder::progress() const
{
    return {
        .spot = _next, .num_spots = (u32)(_spots.size() + _spot_indices_to_recheck.size()),
        .num_failed_dijkstra_calls = _num_failed_dijkstra_calls,
        .all = _all,
    };
}

} // namespace floormat::Cover
