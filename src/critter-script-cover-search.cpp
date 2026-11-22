#include "critter-script-cover-search.hpp"
#include "critter-script-cover-peek-spot.hpp"
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
#include <algorithm>
#include <utility>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat::Cover {

namespace {

constexpr u32 path_slack = 5*(u32)tile_size_xy;
// The work the search may do in one update. Counts, not time, so that the npc behaves the same on every machine.

// Tiles of search_tiles(), each listed by one Cover::Point::finder::find call over the tile's area.
constexpr u32 max_search_tiles_per_update = 32;
// path_to calls, each one Dijkstra search. One that found a path took up to 0.71 ms among dense scenery.
constexpr u32 max_search_dijkstra_calls_per_update = 1;
// Listed cover spots that no reach position sees. Each gets cover_end, up to 0.1 ms among dense
// scenery, and with needs_peek a search for its peek spot, which casts rays and took up to 42 microseconds.
constexpr u32 max_peek_spot_searches_per_update = 1;
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

// at lies anywhere in its chunk, so the cover grids span one chunk more.
u32 search_chunk_radius(point at, point threat, u32 radius, const Cover::Script::options& o)
{
    const auto dist = point::distance(at, threat) + 2*radius + o.threat_reach + o.max_cover_end_dist +
                      (u32)max_peek_spot_dist + (u32)tile_size_xy;
    return dist/chunk_size_xy + 3;
}

cover_search::cover_search() { arrayReserve(_fallback_spots.peekless_spots, 16); }

void cover_search::begin(point from, point threat)
{
    _fallback_spots.clear();
    _exposed_cover = {};
    _dijkstra_search_to_rerun = {};
    arrayClear(_reach_positions_not_seeing_spot.positions);
    arrayClear(_reach_positions_not_seeing_spot.seeing_positions);
    _num_failed_dijkstra_calls = 0;
    _num_failed_dijkstra_calls_for_fallback = 0;
    _pass_gen_at_last_tick = 0;
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
        // At the default max_cover_end_dist and a div_size up to 32, no listed spot lies within a tile of an entry
        // over 3 chunks from _from.
        _unreachable.remove_spots_outside_disc(_from, 3*chunk_size_xy);
        _unreachable_until_rescan.clear();
        _tile = 0;
        _tiles_left--;
        arrayClear(_tile_skipped);
        _octant_tiles = {};
    }
    _octant = Cover::Point::nearest_octant(Vector2(threat - _from));
    _tile_threat = {};
}

auto cover_search::tick(critter& c, spot_tests& t, const Cover::Script::options& o, point from,
                        point threat, u32 max_octant_fills) -> result
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
    const auto max_pass_gen =
        w.max_pass_gen(_from.chunk3(), search_chunk_radius(_from, threat, radius + tile_size_xy, o));
    const auto static_geometry_changed = _pass_gen_at_last_tick && max_pass_gen > _pass_gen_at_last_tick;
    // A scan that ended holds only spots and a path that take_last_resort and take_exposed_cover test again.
    const auto changed = static_geometry_changed && _tile != tiles.size();
    _pass_gen_at_last_tick = w.pass_gen();
    const auto num_body_checks_at_start = t.counters.num_body_checks;
    const auto body_checks_left = [&] {
        return max_search_body_checks_per_update - (t.counters.num_body_checks - num_body_checks_at_start);
    };
    if (_unreachable.is_stale(w, _from))
    {
        _unreachable.clear();
        _num_failed_dijkstra_calls = 0;
        _num_failed_dijkstra_calls_for_fallback = 0;
        arrayClear(_tile_skipped);
        restart(from, threat);
    }
    else if (changed)
    {
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

    // path_to lets no other critter through, so its failure doesn't say whether a critter cut the spot off.
    if (const auto failed_search = _dijkstra_search_to_rerun)
    {
        const auto end = failed_search->end;
        _dijkstra_search_to_rerun = {};
        if (!t.is_reachable_without_critters(c, end))
        {
            _unreachable.add_failed(w, failed_search->spot_pos,
                                    max_search_path_length(here, end) + point::distance(here, _from));
            if (!failed_search->is_fallback_spot)
                _num_failed_dijkstra_calls++;
        }
        return {};
    }
    else if (_tile == tiles.size() || _num_failed_dijkstra_calls >= max_failed_dijkstra_calls)
    {
        // With a spot kept for last_resort, a path kept or a rerun waiting, the search ends in a later update than
        // its last spot, since that spot may have used the update's one Dijkstra call.
        const auto n = t.counters.num_dijkstra_calls;
        if (auto r = take_last_resort(c, t, o, threat, max_pass_gen, body_checks_left()))
            return {move(r), true};
        // Spots take_last_resort kept and a rerun wait for the next update, and so does take_exposed_cover after
        // a Dijkstra search here.
        else if (_fallback_spots.has_cover_spot() || _dijkstra_search_to_rerun ||
                 _exposed_cover && t.counters.num_dijkstra_calls != n)
            return {};
        else if (auto e = take_exposed_cover(c, t, o, threat, max_pass_gen, body_checks_left());
                 e || !_exposed_cover)
            return {move(e), true};
        else
            return {};
    }
    if (_exposed_cover && !_exposed_updates_left)
    {
        if (auto r = take_exposed_cover(c, t, o, threat, max_pass_gen, body_checks_left()))
            return {move(r), true};
        else
            return {};
    }
    if (_exposed_cover)
        _exposed_updates_left--;
    // the offset of _from's tile's center, so that each area is a tile of the world
    const auto base = point{_from.coord(), {}} - _from;
    auto fills = max_octant_fills, dijkstra_left = max_search_dijkstra_calls_per_update;
    auto peek_spot_searches_left = max_peek_spot_searches_per_update;
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
                arrayReserve(_tile_skipped, (_tile_skipped.size() + count)*3/2);
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
            if (!at_spot && _unreachable.can_move_straight_to_listed_spot(w, s.pos, Vector2i(body), pred))
            {
                _unreachable.add(s.pos);
                continue;
            }
            if (!at_spot &&
                _unreachable_until_rescan.can_move_straight_to_listed_spot(w, s.pos, Vector2i(body), pred))
            {
                _unreachable_until_rescan.add(s.pos);
                continue;
            }
            if (!peek_spot_searches_left || !body_checks_left() || !at_spot && !dijkstra_left)
                return {};
            if (o.threat_reach)
            {
                const auto reach = t.reach_positions(c, o, threat);
                auto& fs = _fallback_spots;
                const auto max_count = o.last_resort < fallback::in_reach ? 0u
                                     : fs.in_reach ? fs.num_reach_positions_seeing_in_reach - 1
                                     : (u32)reach.size();
                const auto n = _reach_positions_not_seeing_spot.count_reach_positions_seeing(
                    c, t, reach, s, max_pass_gen, body_checks_left(), max_count);
                if (!n)
                    return {};
                else if (*n)
                {
                    // TODO: weighing this count against the spot's distances from the npc and the threat may
                    // keep a better spot.
                    if (*n <= max_count)
                    {
                        fs.in_reach = s;
                        fs.num_reach_positions_seeing_in_reach = *n;
                    }
                    arrayAppend(_tile_skipped, skipped_spot{s, _octant});
                    if (_exposed_cover && _fallback_spots.has_cover_spot())
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
            const auto peek = o.needs_peek ? t.peek_spot(c, o, end, s.axis, threat)
                                           : Optional<Cover::Script::peek>{};
            if (o.needs_peek && !peek)
            {
                if (o.last_resort >= fallback::no_peek)
                    arrayAppend(_fallback_spots.peekless_spots, s);
                arrayAppend(_tile_skipped, skipped_spot{s, _octant});
                if (_exposed_cover && _fallback_spots.has_cover_spot())
                {
                    _tile = (u32)tiles.size();
                    return {};
                }
                continue;
            }
            if (at_spot || end == here)
                return {reachable_cover{s, end, {}, peek}, true};
            dijkstra_left--;
            // TODO: a Dijkstra search mode that adds the threat seeing the npc to a path's cost, with one cost
            // while the npc retreats and another when it goes to cover from state::retreat.
            if (auto path = t.path_to(c, end); path.is_found())
            {
                if (!t.is_seen_on_path(c, o, threat, path))
                    return {reachable_cover{s, end, move(path), peek}, true};
                arrayAppend(_tile_skipped, skipped_spot{s, _octant});
                if (_exposed_cover)
                    continue;
                _exposed_cover = reachable_cover{s, end, move(path), {}};
                _exposed_updates_left = updates_before_exposed_cover;
                // take_last_resort may call path_to, so it runs in the next update.
                if (_fallback_spots.has_cover_spot())
                {
                    _tile = (u32)tiles.size();
                    return {};
                }
                continue;
            }
            // The npc can walk straight from s.pos to end, so s.pos has no path either.
            _unreachable_until_rescan.add(s.pos);
            if (++_num_failed_dijkstra_calls >= max_failed_dijkstra_calls)
                return {};
            else
            {
                fm_debug_assert(!_dijkstra_search_to_rerun);
                _dijkstra_search_to_rerun = failed_dijkstra_search{s.pos, end};
            }
        }
        _tile_threat = {};
        erase_if(_tile_skipped, [&](const skipped_spot& x) { return x.octant == _octant; });
        _tile++;
        fm_debug_assert(_tiles_left);
        _tiles_left--;
        if (_tile == tiles.size())
            return {{}, !_fallback_spots.has_cover_spot() && !_exposed_cover && !_dijkstra_search_to_rerun};
    }
    return {};
}

Optional<u32> reach_positions_hidden_from::count_reach_positions_seeing(critter& c, spot_tests& t,
                                                                       ArrayView<const point> reach,
                                                                       const Cover::Point::spot& s,
                                                                       u64 max_pass_gen, u32 max_body_checks,
                                                                       u32 max_count)
{
    if (spot.pos != s.pos || spot.axis != s.axis || body_size != c.bbox_size || max_pass_gen > pass_gen)
    {
        spot = s;
        body_size = c.bbox_size;
        arrayClear(positions);
        arrayClear(seeing_positions);
    }
    pass_gen = c.world().pass_gen();
    const ArrayView<const point> hidden = positions, seeing = seeing_positions;
    if (arrayCapacity(next_positions) < reach.size())
        arrayReserve(next_positions, reach.size() + reach.size()/2);
    if (arrayCapacity(next_seeing_positions) < reach.size())
        arrayReserve(next_seeing_positions, reach.size() + reach.size()/2);
    arrayClear(next_positions);
    arrayClear(next_seeing_positions);
    auto count = 0u;
    auto is_unknown = false;
    for (const auto& pt : reach)
    {
        auto is_seeing = std::binary_search(seeing.begin(), seeing.end(), pt);
        if (!is_seeing && !std::binary_search(hidden.begin(), hidden.end(), pt))
        {
            if (!max_body_checks)
            {
                is_unknown = true;
                continue;
            }
            max_body_checks--;
            is_seeing = !t.is_hidden_at(c, pt, s.pos, s.axis);
        }
        if (!is_seeing)
            arrayAppend(next_positions, pt);
        else if (++count > max_count)
            return count;
        else
            arrayAppend(next_seeing_positions, pt);
    }
    std::swap(positions, next_positions);
    std::swap(seeing_positions, next_seeing_positions);
    std::sort(positions.begin(), positions.end());
    std::sort(seeing_positions.begin(), seeing_positions.end());
    if (is_unknown)
        return {};
    else
        return count;
}

auto reach_positions_hidden_from::test_spot_against_reach(critter& c, spot_tests& t, ArrayView<const point> reach,
                                                          const Cover::Point::spot& s, u64 max_pass_gen,
                                                          u32 max_body_checks) -> visibility_from_reach
{
    const auto n = count_reach_positions_seeing(c, t, reach, s, max_pass_gen, max_body_checks, 0);
    if (!n)
        return visibility_from_reach::unknown;
    else if (*n)
        return visibility_from_reach::exposed;
    else
        return visibility_from_reach::hidden;
}

auto cover_search::test_spot_against_reach(critter& c, spot_tests& t, const Cover::Script::options& o,
                                           point threat, const Cover::Point::spot& s, u64 max_pass_gen,
                                           u32 max_body_checks) -> visibility_from_reach
{
    return _reach_positions_not_seeing_spot.test_spot_against_reach(c, t, t.reach_positions(c, o, threat), s,
                                                                    max_pass_gen, max_body_checks);
}

// The kept spot with cover_end() and a path there, if hidden from reach and from the threat in neighbor octants.
// It stays kept when testing that takes over max_body_checks body checks. At most one path_to call.
auto cover_search::take_exposed_cover(critter& c, spot_tests& t, const Cover::Script::options& o, point threat,
                                      u64 max_pass_gen, u32 max_body_checks) -> Optional<reachable_cover>
{
    if (!_exposed_cover)
        return {};
    const auto num_body_checks_at_start = t.counters.num_body_checks;
    auto v = test_spot_against_reach(c, t, o, threat, _exposed_cover->spot, max_pass_gen, max_body_checks);
    if (v == visibility_from_reach::hidden)
    {
        if (t.counters.num_body_checks - num_body_checks_at_start + 2*(o.octants/2u) > max_body_checks)
            return {};
        else if (!t.is_hidden_from_threat_in_neighbor_octants(c, o, threat, _exposed_cover->spot))
            v = visibility_from_reach::exposed;
    }
    if (v == visibility_from_reach::unknown)
        return {};
    else
    {
        auto r = move(*_exposed_cover);
        _exposed_cover = {};
        if (v == visibility_from_reach::exposed)
            return {};
        else
        {
            const auto s = r.spot;
            const auto end = t.cover_end(c, o, s, threat);
            const auto here = c.position();
            if (s.pos == here || end == here)
                return reachable_cover{s, end, {}, {}};
            else if (const auto raw = r.path.path(); end == r.end && !raw.isEmpty() && raw.front() == here)
                return r;
            else if (auto path = t.path_to(c, end); path.is_found())
                return reachable_cover{s, end, move(path), {}};
            else
                return {};
        }
    }
}

// The first of peekless_spots hidden from reach, else in_reach if hidden from the threat. Each must be hidden from
// the threat in neighbor octants. Spots max_body_checks can't test, and those after one with no path, stay kept.
auto cover_search::take_last_resort(critter& c, spot_tests& t, const Cover::Script::options& o, point threat,
                                    u64 max_pass_gen, u32 max_body_checks) -> Optional<reachable_cover>
{
    auto& w = c.world();
    auto& peekless_spots = _fallback_spots.peekless_spots;
    const auto in_reach = _fallback_spots.in_reach;
    const auto body = Vector2i(c.bbox_size);
    const auto pred = Search::blocks_all_but(c.id);
    const auto num_body_checks_at_start = t.counters.num_body_checks;
    const auto body_checks_left = [&] {
        return max_body_checks - (t.counters.num_body_checks - num_body_checks_at_start);
    };
    const auto num_neighbor_octant_body_checks = 2*(o.octants/2u);
    auto is_hidden_from_reach = false;
    while (!is_hidden_from_reach && !peekless_spots.isEmpty())
    {
        const auto s = peekless_spots[0];
        if (_unreachable.can_move_straight_to_listed_spot(w, s.pos, body, pred) ||
            _unreachable_until_rescan.can_move_straight_to_listed_spot(w, s.pos, body, pred))
            arrayRemove(peekless_spots, 0);
        else if (const auto v = test_spot_against_reach(c, t, o, threat, s, max_pass_gen, body_checks_left());
                 v == visibility_from_reach::unknown ||
                 v == visibility_from_reach::hidden && body_checks_left() < num_neighbor_octant_body_checks)
            return {};
        else if (v == visibility_from_reach::hidden &&
                 t.is_hidden_from_threat_in_neighbor_octants(c, o, threat, s))
            is_hidden_from_reach = true;
        else
            arrayRemove(peekless_spots, 0);
    }
    if (!is_hidden_from_reach && in_reach && body_checks_left() < 1 + num_neighbor_octant_body_checks)
        return {};
    else
    {
        Optional<Cover::Point::spot> spot;
        if (is_hidden_from_reach)
        {
            spot = peekless_spots[0];
            arrayRemove(peekless_spots, 0);
        }
        else
        {
            _fallback_spots.in_reach = {};
            if (in_reach && t.is_hidden_at(c, threat, in_reach->pos, in_reach->axis) &&
                t.is_hidden_from_threat_in_neighbor_octants(c, o, threat, *in_reach))
                spot = in_reach;
        }
        if (!spot)
            return {};
        else
        {
            const auto s = *spot;
            const auto end = t.cover_end(c, o, s, threat);
            const auto here = c.position();
            if (s.pos == here || end == here)
            {
                _fallback_spots.clear();
                return reachable_cover{s, end, {}, {}, is_hidden_from_reach};
            }
            else if (auto path = t.path_to(c, end); path.is_found())
            {
                _fallback_spots.clear();
                return reachable_cover{s, end, move(path), {}, is_hidden_from_reach};
            }
            else
            {
                _unreachable_until_rescan.add(s.pos);
                if (++_num_failed_dijkstra_calls_for_fallback < max_failed_dijkstra_calls)
                {
                    fm_debug_assert(!_dijkstra_search_to_rerun);
                    _dijkstra_search_to_rerun = failed_dijkstra_search{s.pos, end, true};
                }
                else
                    arrayClear(peekless_spots);
                return {};
            }
        }
    }
}

void cover_search::end() { _tiles_left = 0; }
void cover_search::give_up() { _tile = (u32)search_tiles().size(); }

bool cover_search::is_running() const { return _tiles_left != 0; }
point cover_search::from() const { return _from; }

Cover::Script::search_progress cover_search::progress() const
{
    return {.tile = _tile, .tiles_left = _tiles_left, .num_tiles = (u32)search_tiles().size()};
}

bool cover_search::has_last_resort() const { return _fallback_spots.has_cover_spot(); }

bool cover_search::fallback_spots::has_cover_spot() const { return !peekless_spots.isEmpty() || in_reach; }

void cover_search::fallback_spots::clear()
{
    arrayClear(peekless_spots);
    in_reach = {};
    num_reach_positions_seeing_in_reach = 0;
}

} // namespace floormat::Cover
