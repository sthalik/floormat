#include "critter-script-cover-search.hpp"
#include "critter-script-cover-point.hpp"
#include "critter-script-cover-spot-tests.hpp"
#include "critter.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "tile-defs.hpp"
#include "search-pred.hpp"
#include "world.hpp"
#include "compat/assert.hpp"
#include "compat/container-traits.inl"
#include "compat/erase-if.hpp"
#include "compat/function-view.hpp"
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat::Cover {

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
// Updates the search goes on for after keeping a path that is_seen_on_path rejected, before it takes that path.
constexpr u32 updates_before_exposed_cover = 4;

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
    _fallback_spots = {};
    _exposed_cover = {};
    _num_failed_dijkstra_calls = 0;
    const auto n = (u32)search_tiles().size();
    _tiles_left = n + n/2;
    restart(from, threat);
}

void cover_search::restart(point from, point threat)
{
    _exposed_cover = {};
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
    _tile_threat = {};
}

auto cover_search::tick(critter& c, spot_tests& t, const Cover::Script::options& o, point from,
                        point threat) -> result
{
    using fallback = Cover::Script::options::fallback;
    auto& w = c.world();
    const auto here = c.position();
    const auto body = Vector2ui(c.bbox_size);
    const auto pred = Search::blocks_all_but(c.id);
    // Listed spots hold only for the threat position they were listed for, and a Dijkstra search's result
    // only for the pass grids it ran in.
    const auto k = Cover::Point::nearest_octant(Vector2(threat - _from));
    const auto& tiles = search_tiles();
    if (_unreachable.stale(w, _from))
    {
        _unreachable.clear();
        _num_failed_dijkstra_calls = 0;
        arrayClear(_tile_skipped);
        restart(from, threat);
    }
    else if (k != _octant)
    {
        _octant_tiles[_octant] = (u16)_tile;
        if (from == _from && _tiles_left > tiles.size() - _octant_tiles[k] + 1)
        {
            _octant = k;
            _exposed_cover = {};
            _tile = _octant_tiles[k];
            _tiles_left--;
            _tile_threat = {};
        }
        else
            restart(from, threat);
    }
    else if (_tile_threat != threat)
        _tile_threat = {};

    // With a spot kept for last_resort or a path kept, the search ends in the update after its last spot, since
    // that spot may have used the update's one Dijkstra call.
    if (_tile == tiles.size() || _num_failed_dijkstra_calls == max_failed_dijkstra_calls)
    {
        const auto n = t.counters.num_dijkstra_calls;
        auto r = take_last_resort(c, t, o, threat);
        // take_exposed_cover may call path_to too, so it waits for the next update after a path_to call here.
        if (!r && _exposed_cover && t.counters.num_dijkstra_calls != n)
            return {};
        else if (!r)
            return {take_exposed_cover(c, t, o, threat), true};
        else
            return {move(r), true};
    }
    if (_exposed_cover && !_exposed_updates_left)
    {
        if (auto r = take_exposed_cover(c, t, o, threat))
            return {move(r), true};
        else
            return {};
    }
    if (_exposed_cover)
        _exposed_updates_left--;
    // the offset of _from's tile's center, so that each area is a tile of the world
    const auto base = point{_from.coord(), {}} - _from;
    auto fills = max_search_octant_fills_per_update, dijkstra_left = max_search_dijkstra_calls_per_update;
    auto peek_spot_searches_left = max_peek_spot_searches_per_update;
    u32 num_checks = 0;
    for (auto n = 0u; n < max_search_tiles_per_update; n++)
    {
        if (!_tile_threat)
        {
            const auto mid = base + tiles[_tile]*tile_size_xy;
            const Cover::Point::query q{_from, body, threat, radius, o.octants, o.listed_cover_end_dist(),
                                        Range2Di::fromCenter(mid, Vector2i{half_tile<int>}), fills};
            const auto r = t.finder().find(w, t.pool_registry(), Grid::Cover::Params{o.div_size}, q, pred, true);
            t.counters.num_find_cover_point_calls++;
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
                const auto n = t.counters.num_body_checks;
                const auto hidden = t.hidden_from_reach(c, o, threat, s.pos, s.axis);
                num_checks += t.counters.num_body_checks - n;
                if (!hidden)
                {
                    if (o.last_resort >= fallback::in_reach && !_fallback_spots.in_reach)
                        _fallback_spots.in_reach = s;
                    arrayAppend(_tile_skipped, skipped_spot{s, _octant});
                    if (_exposed_cover && _fallback_spots.any())
                    {
                        _tile = (u32)tiles.size();
                        return {};
                    }
                    continue;
                }
            }
            peek_spot_searches_left--;
            // A path to s.pos could pass end and come back.
            const auto end = t.cover_end(c, o, s, threat);
            if (o.needs_peek && !t.peek_spot(c, o, end, s.axis, threat))
            {
                if (o.last_resort >= fallback::no_peek && !_fallback_spots.peekless)
                    _fallback_spots.peekless = s;
                arrayAppend(_tile_skipped, skipped_spot{s, _octant});
                if (_exposed_cover && _fallback_spots.any())
                {
                    _tile = (u32)tiles.size();
                    return {};
                }
                continue;
            }
            if (at_spot || end == here)
                return {reachable_cover{s, end, {}}, true};
            dijkstra_left--;
            // TODO: a Dijkstra search mode that adds the threat seeing the npc to a path's cost, with one cost
            // while the npc retreats and another when it goes to cover from state::retreat.
            if (auto path = t.path_to(c, end); path.is_found())
            {
                if (!t.is_seen_on_path(c, o, threat, path))
                    return {reachable_cover{s, end, move(path)}, true};
                arrayAppend(_tile_skipped, skipped_spot{s, _octant});
                if (_exposed_cover)
                    continue;
                _exposed_cover = reachable_cover{s, end, move(path)};
                _exposed_updates_left = updates_before_exposed_cover;
                // take_last_resort may call path_to, so it runs in the next update.
                if (_fallback_spots.any())
                {
                    _tile = (u32)tiles.size();
                    return {};
                }
                continue;
            }
            // The npc can walk straight from s.pos to end, so s.pos has no path either.
            const auto reach = max_search_path_length(here, end) + point::distance(here, _from);
            _unreachable.add_failed(w, s.pos, reach);
            if (++_num_failed_dijkstra_calls == max_failed_dijkstra_calls)
                return {{}, !_fallback_spots.any() && !_exposed_cover};
        }
        _tile_threat = {};
        erase_if(_tile_skipped, [&](const skipped_spot& x) { return x.octant == _octant; });
        _tile++;
        fm_debug_assert(_tiles_left);
        _tiles_left--;
        if (_tile == tiles.size())
            return {{}, !_fallback_spots.any() && !_exposed_cover};
    }
    return {};
}

// The kept spot if the threat and where it can walk still see none of the npc there, with cover_end() and a path
// there. At most one path_to call, made when the npc or cover_end() moved.
auto cover_search::take_exposed_cover(critter& c, spot_tests& t, const Cover::Script::options& o, point threat)
    -> Optional<reachable_cover>
{
    if (!_exposed_cover)
        return {};
    auto r = move(*_exposed_cover);
    _exposed_cover = {};
    const auto s = r.spot;
    if (!t.hidden_from_reach(c, o, threat, s.pos, s.axis))
        return {};
    else
    {
        const auto end = t.cover_end(c, o, s, threat);
        const auto here = c.position();
        if (s.pos == here || end == here)
            return reachable_cover{s, end, {}};
        else if (const auto raw = r.path.path(); end == r.end && !raw.isEmpty() && raw.front() == here)
            return r;
        else if (auto path = t.path_to(c, end); path.is_found())
            return reachable_cover{s, end, move(path)};
        else
            return {};
    }
}

// peekless if the threat and where it can walk still see none of the npc there, else in_reach if the threat
// alone doesn't, with its cover_end() and a path there. Clears both. At most one path_to call, as in tick().
auto cover_search::take_last_resort(critter& c, spot_tests& t, const Cover::Script::options& o,
                                    point threat) -> Optional<reachable_cover>
{
    const auto [peekless, in_reach] = _fallback_spots;
    _fallback_spots = {};
    Optional<Cover::Point::spot> spot;
    if (peekless)
    {
        const auto& s = *peekless;
        if (t.hidden_from_reach(c, o, threat, s.pos, s.axis))
            spot = s;
    }
    if (!spot && in_reach)
    {
        const auto& s = *in_reach;
        if (t.hidden_at(c, threat, s.pos, s.axis))
            spot = s;
    }
    if (!spot)
        return {};
    const auto s = *spot;
    const auto end = t.cover_end(c, o, s, threat);
    if (const auto here = c.position(); s.pos == here || end == here)
        return reachable_cover{s, end, {}};
    if (auto path = t.path_to(c, end); path.is_found())
        return reachable_cover{s, end, move(path)};
    return {};
}

void cover_search::end() { _tiles_left = 0; }
void cover_search::give_up() { _tile = (u32)search_tiles().size(); }

void cover_search::options_changed(Cover::Script::options::fallback last_resort)
{
    _fallback_spots.drop(last_resort);
    _exposed_cover = {};
    arrayClear(_tile_skipped);
}

bool cover_search::running() const { return _tiles_left != 0; }
point cover_search::from() const { return _from; }

Cover::Script::search_progress cover_search::progress() const
{
    return {.tile = _tile, .tiles_left = _tiles_left, .num_tiles = (u32)search_tiles().size()};
}

bool cover_search::has_last_resort() const { return _fallback_spots.any(); }

void cover_search::fallback_spots::drop(Cover::Script::options::fallback last_resort)
{
    using fallback = Cover::Script::options::fallback;
    switch (last_resort)
    {
    case fallback::none:
        peekless = {};
        in_reach = {};
        return;
    case fallback::no_peek:
        in_reach = {};
        return;
    case fallback::in_reach:
        return;
    }
    fm_assert(false);
}

bool cover_search::fallback_spots::any() const { return peekless || in_reach; }

} // namespace floormat::Cover
