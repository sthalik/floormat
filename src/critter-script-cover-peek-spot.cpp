#include "critter-script-cover-peek-spot.hpp"
#include "critter-script-cover.hpp"
#include "critter.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "search.hpp"
#include "tile-defs.hpp"
#include "compat/function2.hpp"
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

constexpr int max_peek_dist = 2*tile_size_xy;

} // namespace

point gun_point(const critter& c, point pos, point threat, Vector2i across, Optional<int> offset)
{
    if (pos == threat)
        return pos;
    const auto dist = offset ? *offset : Math::dot(Vector2i(c.bbox_size), across)/2;
    const auto f = Vector2(threat - pos);
    return pos + Vector2i(Math::round(f.perpendicular() * ((float)dist / f.length())));
}

Optional<Cover::Script::peek> peek_spot(critter& c, point from, Vector2i axis, point threat, Optional<int> gun_offset,
                                        const Search::pred& pred, fu2::function_view<bool(point)> sees)
{
    auto& w = c.world();
    const auto body = Vector2i(c.bbox_size);
    const auto across = Cover::Point::across_of(axis);
    // The spot is flush against a blocker along axis, so the last three run into it on the first
    // step and cast no ray.
    const Vector2i dirs[] = {
        across, -across, -axis, across - axis, -across - axis, axis, across + axis, axis - across,
    };

    Vector2i best;
    int best_len2 = 0;
    for (const auto dir : dirs)
    {
        const auto diagonal = dir.x() && dir.y();
        const auto step = Math::max(1, (diagonal ? Math::min(body.x(), body.y()) : Math::dot(body, Math::abs(dir)))/2);
        const auto len2 = [&](int t) { return t*t*Math::dot(dir, dir); };
        const auto seen = [&](int t) { return sees(gun_point(c, from + dir*t, threat, across, gun_offset)); };
        // A walk from `from` along dir sweeps no more than the box spanning both ends. On a diagonal, that box
        // also holds two corners the walk never reaches, so a collider there rejects a clear walk.
        const auto clear = [&](int t) {
            const auto lo = from.intra().center_shifted() - body/2 + Math::min(dir*t, Vector2i{});
            const auto size = body + Math::abs(dir*t);
            return Search::is_passable(w, from.chunk3(), Range2D{Vector2(lo), Vector2(lo + size)}, pred);
        };

        int lo = 0, hi = 0;
        for (auto t = step; len2(t) <= max_peek_dist*max_peek_dist && !hi; t += step)
        {
            if (!clear(t))
            {
                // A longer walk's box holds a shorter one's, so the clear walks end before t.
                auto clear_t = lo, blocked_t = t;
                while (blocked_t - clear_t > 1)
                {
                    const auto mid = (clear_t + blocked_t)/2;
                    if (clear(mid))
                        clear_t = mid;
                    else
                        blocked_t = mid;
                }
                if (clear_t > lo && seen(clear_t))
                    hi = clear_t;
                break;
            }
            if (seen(t))
                hi = t;
            else
            {
                lo = t;
                if (best_len2 && len2(t) >= best_len2)
                    break;
            }
        }
        if (!hi)
            continue;
        while (hi - lo > 1)
        {
            const auto mid = (lo + hi)/2;
            if (seen(mid))
                hi = mid;
            else
                lo = mid;
        }
        if (!best_len2 || len2(hi) < best_len2)
        {
            best = dir*hi;
            best_len2 = len2(hi);
        }
    }
    if (!best_len2)
        return {};
    const auto pos = from + best;
    return Cover::Script::peek{pos, gun_point(c, pos, threat, across, gun_offset)};
}

} // namespace floormat
