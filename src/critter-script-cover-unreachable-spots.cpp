#include "critter-script-cover-unreachable-spots.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "search.hpp"
#include "tile-defs.hpp"
#include "world.hpp"
#include "compat/nanosecond.inl"
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat::Cover {

unreachable_spots::unreachable_spots() { arrayReserve(_spots, 16); }

void unreachable_spots::clear()
{
    arrayClear(_spots);
    _reach = 0;
}

bool unreachable_spots::joins(world& w, point to, Vector2i body, const Search::pred& pred) const
{
    for (const auto& u : _spots)
    {
        const auto d = to - u;
        if (Math::abs(d).max() > tile_size_xy)
            continue;
        const auto a = u.intra().center_shifted(), b = a + d;
        const auto lo = Math::min(a, b) - body/2;
        const auto hi = Math::max(a, b) - body/2 + body;
        if (Search::is_passable(w, u.chunk3(), Range2D{Vector2(lo), Vector2(hi)}, pred))
            return true;
    }
    return false;
}

void unreachable_spots::add(point pos)
{
    bool known = false;
    for (const auto& u : _spots)
        known |= u == pos;
    if (!known)
        arrayAppend(_spots, pos);
}

void unreachable_spots::add_failed(const world& w, point pos, u32 reach)
{
    if (_spots.isEmpty())
        _expires_at = w.game_time() + unreachable_spot_retry_time;
    arrayAppend(_spots, pos);
    _reach = Math::max(_reach, reach);
    _pass_gen = w.pass_gen();
}

void unreachable_spots::origin_moved(u32 dist)
{
    if (!_spots.isEmpty())
        _reach += dist;
}

bool unreachable_spots::is_stale(const world& w, point search_origin) const
{
    if (_spots.isEmpty())
        return false;
    if (w.game_time() >= _expires_at)
        return true;
    auto reach = _reach;
    for (const auto& u : _spots)
        reach = Math::max(reach, point::distance(search_origin, u));
    // search_origin lies anywhere in its chunk, so the reach spans one chunk more than its length.
    return w.max_pass_gen(search_origin.chunk3(), reach / chunk_size_xy + 2) > _pass_gen;
}

} // namespace floormat::Cover
