#include "critter-script-cover-retreat-finder.hpp"
#include "critter-script-cover-point.hpp"
#include "grid-cover.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "search.hpp"
#include "tile-defs.hpp"
#include "world.hpp"
#include "compat/assert.hpp"
#include "compat/function2.hpp"
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
// sees() calls, each one ray from the threat to a spot's center.
constexpr u32 max_retreat_rays_per_update = 256;
// Search::is_passable calls for the npc's body at a spot, each an R-tree search.
constexpr u32 max_retreat_is_passable_calls_per_update = 512;

static_assert(Grid::Cover::octant_count <= 32);

} // namespace

void retreat_finder::begin(Vector2i body, u32 threat_octant, bool restart)
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
        s.done = false;
        s.seen_octants = 0;
        _rays += s.offset.dot() > min_retreat_gain*min_retreat_gain;
    }
    _next = 0;
    _num_failed_dijkstra_calls = 0;
    _all = false;
    _octant = threat_octant;
    _num_restarts += restart;
}

retreat_finder::result retreat_finder::step(world& w, point from, point threat, Vector2i body,
                                            const Search::pred& pred, u32 max_failed_dijkstra_calls,
                                            fu2::function_view<bool(point)> sees,
                                            fu2::function_view<path_search_result(point)> path_to)
{
    if (const auto k = Cover::Point::nearest_octant(Vector2(threat - from)); k != _octant)
    {
        // Spots the threat saw from its old octant may be hidden now. The pass and its failed Dijkstra calls stay,
        // so a moving threat can't cause more than 2*max_failed_dijkstra_calls of them.
        _octant = k;
        _next = 0;
        _num_restarts++;
    }
    const auto octant_bit = 1u << _octant;
    const auto here = Math::sqrt((double)point::distance_squared(from, threat));
    auto dijkstra_left = max_retreat_dijkstra_calls_per_update, rays_left = max_retreat_rays_per_update;
    auto passable_left = max_retreat_is_passable_calls_per_update;
    for (;;)
    {
        if (_next == _spots.size() || _num_failed_dijkstra_calls == max_failed_dijkstra_calls || !_all && !_rays)
        {
            if (_all)
                return {{}, true};
            _all = true;
            _next = 0;
            _num_failed_dijkstra_calls = 0;
            continue;
        }
        auto& s = _spots[_next];
        const auto to = from + s.offset;
        if (s.done || !_all && (s.seen_octants & octant_bit))
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
        if (!passable_left)
            return {};
        passable_left--;
        const auto lo = Vector2(to.intra().center_shifted() - body/2);
        if (!Search::is_passable(w, to.chunk3(), Range2D::fromSize(lo, Vector2(body)), pred))
        {
            s.done = true;
            _next++;
            continue;
        }
        if (!dijkstra_left)
            return {};
        if (!_all)
        {
            if (!rays_left)
                return {};
            rays_left--;
            _rays--;
            if (sees(to))
            {
                s.seen_octants |= octant_bit;
                _next++;
                continue;
            }
        }
        dijkstra_left--;
        s.done = true;
        _next++;
        if (auto path = path_to(to); path.is_found())
            return {move(path), false};
        _num_failed_dijkstra_calls++;
    }
}

bool retreat_finder::keeps(point to, point threat, fu2::function_view<bool(point)> sees) const
{
    fm_debug_assert(_next > 0);
    const auto from = to - _spots[_next - 1].offset;
    const auto dist = [&](point p) { return Math::sqrt((double)point::distance_squared(p, threat)); };
    return dist(to) - dist(from) > min_retreat_gain && (_all || !sees(to));
}

u32 retreat_finder::next() const { return _next; }
u32 retreat_finder::num_spots() const { return (u32)_spots.size(); }
u32 retreat_finder::num_failed_dijkstra_calls() const { return _num_failed_dijkstra_calls; }
bool retreat_finder::all() const { return _all; }
u32 retreat_finder::num_restarts() const { return _num_restarts; }

} // namespace floormat::Cover
