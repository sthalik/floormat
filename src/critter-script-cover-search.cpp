#include "critter-script-cover-search.hpp"
#include "cover-point.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "tile-defs.hpp"
#include "world.hpp"
#include "compat/assert.hpp"
#include <algorithm>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

constexpr u32 path_slack = 5*(u32)tile_size_xy;
// The work the search may do in one update. Counts, not time, so that the npc behaves the same on every machine.
// TODO: each critter has its own limits, so n critters searching in different chunks can do n
// octant fills in one frame.

// Tiles of search_tiles(), each listed by one Cover::Point::finder::find call over the tile's area.
constexpr u32 max_search_tiles_per_update = 32;
// An octant fill computes how far a shot travels from each cell of a chunk's cover grid along one of 32
// directions. A stale grid's rebuild counts as one. A rebuild and a fill took 0.66 ms at div 8.
constexpr u32 max_search_octant_fills_per_update = 1;
// path_to calls, each one Dijkstra search. One that found a path took up to 0.71 ms among dense scenery.
constexpr u32 max_search_dijkstra_calls_per_update = 1;
// Listed cover spots that hidden_from_reach doesn't reject. Each gets cover_end, up to 0.1 ms among dense
// scenery, and with needs_peek a search for its peek spot, which casts rays and took up to 42 microseconds.
constexpr u32 max_peek_spot_searches_per_update = 1;
// hidden_at calls made by hidden_from_reach, each up to 6 microseconds among dense scenery. Rejecting a spot
// may take a single call, so a spot that hidden_from_reach rejects counts against this cap only.
constexpr u32 max_search_body_checks_per_update = 64;

// A query's pos lies up to half a tile from its tile's center on each axis, so the list reaches a
// tile past cover_search::radius.
const Array<Vector2i>& search_tiles()
{
    static const auto tiles = [] {
        Array<Vector2i> a;
        Cover::Point::nearest_tiles(cover_search::radius + tile_size_xy, a);
        fm_assert(a.size() <= UINT16_MAX);
        return a;
    }();
    return tiles;
}

} // namespace

u32 max_search_path_length(point from, point to) { return point::distance(from, to)*2 + path_slack; }

void cover_search::begin(point from, point threat)
{
    _peekless = {};
    _in_reach = {};
    _unreachable.clear();
    const auto n = (u32)search_tiles().size();
    _tiles_left = n + n/2;
    restart(from, threat);
}

void cover_search::restart(point from, point threat)
{
    // More than a scan stays left after the restart's tile, so the count stays nonzero until the search ends.
    if (_tiles_left > search_tiles().size() + 1)
    {
        _unreachable.origin_moved(point::distance(_from, from));
        _from = from;
        _tile = 0;
        _tiles_left--;
        arrayClear(_tile_skipped);
        _octant_tiles = {};
    }
    _octant = Cover::Point::nearest_octant(Vector2(threat - _from));
    _tile_listed = false;
}

auto cover_search::step(world& w, Cover::Point::finder& finder, Grid::GridRegistry& pools, point here, point from,
                        point threat, Vector2ui body, const Search::pred& pred, const Cover::Script::options& o,
                        u32 max_failed_dijkstra_calls, const calls& f) -> result
{
    using fallback = Cover::Script::options::fallback;
    // Listed spots hold only for the threat position they were listed for, and a Dijkstra search's result
    // only for the pass grids it ran in.
    const auto k = Cover::Point::nearest_octant(Vector2(threat - _from));
    const auto& tiles = search_tiles();
    if (_unreachable.stale(w, _from))
    {
        _unreachable.clear();
        arrayClear(_tile_skipped);
        restart(from, threat);
    }
    else if (k != _octant)
    {
        _octant_tiles[_octant] = (u16)_tile;
        if (from == _from && _tiles_left > tiles.size() - _octant_tiles[k] + 1)
        {
            _octant = k;
            _tile = _octant_tiles[k];
            _tiles_left--;
            _tile_listed = false;
        }
        else
            restart(from, threat);
    }
    else if (threat != _tile_threat)
        _tile_listed = false;

    // With a spot kept for last_resort, the search ends in the update after its last spot, since that spot may
    // have used the update's one Dijkstra call.
    if (_tile == tiles.size() || _unreachable.num_failed_dijkstra_calls() == max_failed_dijkstra_calls)
        return {take_last_resort(here, f), true};
    // the offset of _from's tile's center, so that each area is a tile of the world
    const auto base = point{_from.coord(), {}} - _from;
    auto fills = max_search_octant_fills_per_update, dijkstra_left = max_search_dijkstra_calls_per_update;
    auto peek_spot_searches_left = max_peek_spot_searches_per_update;
    u32 num_checks = 0;
    for (auto n = 0u; n < max_search_tiles_per_update; n++)
    {
        if (!_tile_listed)
        {
            const auto mid = base + tiles[_tile]*tile_size_xy;
            const Cover::Point::query q{_from, body, threat, radius, o.octants, o.listed_cover_end_dist(),
                                        Range2Di::fromCenter(mid, Vector2i{half_tile<int>}), fills};
            const auto r = finder.find(w, pools, Grid::Cover::Params{o.div_size}, q, pred, true);
            _num_find_cover_point_calls++;
            fills -= r.fills;
            if (r.out_of_fills)
                return {};
            const auto count = r.spots.size();
            if (arrayCapacity(_tile_spots) < count)
                arrayReserve(_tile_spots, count + count/2);
            arrayClear(_tile_spots);
            arrayAppend(_tile_spots, r.spots);
            if (arrayCapacity(_tile_skipped) < _tile_skipped.size() + count)
                arrayReserve(_tile_skipped, _tile_skipped.size() + count);
            _tile_spot = 0;
            _tile_listed = true;
            _tile_threat = threat;
        }
        for (; _tile_spot < _tile_spots.size(); _tile_spot++)
        {
            const auto s = _tile_spots[_tile_spot];
            bool skipped = false;
            for (const auto& x : _tile_skipped)
                skipped |= x.spot.pos == s.pos && x.spot.axis == s.axis && x.octant == _octant;
            if (skipped)
                continue;
            const auto at_spot = s.pos == here;
            if (!at_spot && _unreachable.joins(w, s.pos, Vector2i(body), pred))
            {
                _unreachable.add(s.pos);
                continue;
            }
            if (!peek_spot_searches_left || num_checks >= max_search_body_checks_per_update ||
                !at_spot && !dijkstra_left)
                return {};
            if (o.threat_reach)
            {
                const auto h = f.hidden_from_reach(s.pos, s.axis);
                num_checks += h.num_body_checks;
                if (!h.hidden)
                {
                    if (o.last_resort >= fallback::in_reach && !_in_reach)
                        _in_reach = s;
                    arrayAppend(_tile_skipped, skipped_spot{s, _octant});
                    continue;
                }
            }
            peek_spot_searches_left--;
            // A path to s.pos could pass end and come back.
            const auto end = f.cover_end(s);
            if (o.needs_peek && !f.has_peek(end, s.axis))
            {
                if (o.last_resort >= fallback::no_peek && !_peekless)
                    _peekless = s;
                arrayAppend(_tile_skipped, skipped_spot{s, _octant});
                continue;
            }
            if (at_spot || end == here)
                return {reachable_cover{s, end, {}}, true};
            dijkstra_left--;
            // TODO: the path may lead toward the threat. Avoiding it needs A.Dijkstra predicate changes and maybe
            // a path to the threat, or another measure of how close the path comes to it.
            if (auto path = f.path_to(end); path.is_found())
                return {reachable_cover{s, end, move(path)}, true};
            // The npc can walk straight from s.pos to end, so s.pos has no path either.
            const auto reach = max_search_path_length(here, end) + point::distance(here, _from);
            _unreachable.add_failed(s.pos, reach, w.pass_gen());
            if (_unreachable.num_failed_dijkstra_calls() == max_failed_dijkstra_calls)
                return {{}, !_peekless && !_in_reach};
        }
        _tile_listed = false;
        const auto kept = std::remove_if(_tile_skipped.begin(), _tile_skipped.end(),
                                         [&](const skipped_spot& x) { return x.octant == _octant; });
        arrayRemoveSuffix(_tile_skipped, (usize)(_tile_skipped.end() - kept));
        _tile++;
        fm_debug_assert(_tiles_left);
        _tiles_left--;
        if (_tile == tiles.size())
            return {{}, !_peekless && !_in_reach};
    }
    return {};
}

// _peekless if the threat and where it can walk still see none of the npc there, else _in_reach if the threat
// alone doesn't, with its cover_end() and a path there. Clears both. At most one path_to call, as in step().
auto cover_search::take_last_resort(point here, const calls& f) -> Optional<reachable_cover>
{
    Optional<Cover::Point::spot> spot;
    if (_peekless && f.hidden_from_reach(_peekless->pos, _peekless->axis).hidden)
        spot = _peekless;
    else if (_in_reach && f.hidden_at(_in_reach->pos, _in_reach->axis))
        spot = _in_reach;
    _peekless = {};
    _in_reach = {};
    if (!spot)
        return {};
    const auto s = *spot;
    const auto end = f.cover_end(s);
    if (s.pos == here || end == here)
        return reachable_cover{s, end, {}};
    if (auto path = f.path_to(end); path.is_found())
        return reachable_cover{s, end, move(path)};
    return {};
}

void cover_search::end() { _tiles_left = 0; }

void cover_search::drop_last_resort(Cover::Script::options::fallback last_resort)
{
    using fallback = Cover::Script::options::fallback;
    switch (last_resort)
    {
    case fallback::none:
        _peekless = {};
        _in_reach = {};
        return;
    case fallback::no_peek:
        _in_reach = {};
        return;
    case fallback::in_reach:
        return;
    }
    fm_assert(false);
}

void cover_search::forget_skipped() { arrayClear(_tile_skipped); }

point cover_search::from() const { return _from; }
u32 cover_search::tile() const { return _tile; }
u32 cover_search::tiles_left() const { return _tiles_left; }
u32 cover_search::num_tiles() { return (u32)search_tiles().size(); }
u32 cover_search::num_find_cover_point_calls() const { return _num_find_cover_point_calls; }
bool cover_search::has_last_resort() const { return _peekless || _in_reach; }

} // namespace floormat
