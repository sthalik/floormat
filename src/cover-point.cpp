#include "cover-point.hpp"
#include "grid-cover.hpp"
#include "grid-pass.hpp"
#include "grid-pool-registry.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "raycast.hpp"
#include "search.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "compat/floor-divmod.hpp"
#include <algorithm>
#include <cmath>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

uint32_t octant_toward(Vector2 v)
{
    constexpr auto n = (int)Cover::octant_count;
    constexpr float step = 2.f * Math::Constants<float>::pi() / (float)n;
    const auto k = (int)std::lround(std::atan2(v.y(), v.x()) / step);
    return (uint32_t)((k % n + n) % n);
}

struct candidate
{
    Vector2i center;
    float walk_dist;
    uint32_t blocker_px;
};

struct cell_info
{
    bool hidden, blocked;
    uint16_t distance;
};

struct exposure
{
    bool minus, plus;
};

struct placed
{
    Vector2i pos;
    exposure e;
};

} // namespace

cover_point_result find_cover_point(world& w, Grid::PoolRegistry& pools, Cover::Params params, const cover_query& q)
{
    fm_assert(q.pos.chunk3().z == q.threat.chunk3().z);
    auto& cover_pool = pools.cover_pool(params);
    auto& ray_pool = pools.pass_pool(Pass::Params{params.div_size, pass_mode::shoot_through});
    cover_pool.maybe_mark_stale_all(w);

    const auto ds = (int)cover_pool.params().div_size;
    const auto dc = chunk_size_xy / (uint32_t)ds;
    const auto P = Vector3i(q.pos).xy(), T = Vector3i(q.threat).xy();
    const auto half = Vector2(q.body_size) * .5f;
    const auto R = (int)q.radius;
    const auto to_point = [&](Vector2i px) { return q.pos + (px - P); };

    // cell n spans px n*ds - half_tile up to the next one, so cells line up with chunk edges
    const auto cell_of = [ds](Vector2i px) {
        return Vector2i{floor_div(px.x() + half_tile<int>, ds), floor_div(px.y() + half_tile<int>, ds)};
    };
    const auto center_of = [ds](Vector2i n) { return n*ds + Vector2i{ds/2 - half_tile<int>}; };

    const auto margin = Vector2i{Math::ceil(half)} + Vector2i{R};
    const auto lo = cell_of(P - margin), span = cell_of(P + margin) - lo + Vector2i{1};
    Array<uint8_t> known{ValueInit, (size_t)(span.x()*span.y())};
    Array<cell_info> cells{NoInit, (size_t)(span.x()*span.y())};

    const auto look = [&](Vector2i n) -> cell_info {
        const auto i = (size_t)((n.y() - lo.y())*span.x() + n.x() - lo.x());
        if (known[i])
            return cells[i];
        known[i] = true;
        cells[i] = { false, false, Cover::blocked };
        const auto center = center_of(n);
        const auto pt = to_point(center);
        auto* c = w.at(pt.chunk3());
        const auto v = Vector2(T - center);
        const auto len = v.length();
        if (!c || len < 1)
            return cells[i];
        auto g = cover_pool[*c];
        g.build_if_stale();
        const auto k = octant_toward(v);
        (void)g.ensure_octant(k);
        const auto d = g.distance(g.get_cell_index_from_coord(pt.intra()), k);
        // dc is also a miss, so a threat past the cap reads as exposed
        cells[i] = { d != Cover::blocked && d < dc && (float)(d*(uint32_t)ds) < len, d == Cover::blocked, d };
        return cells[i];
    };

    const auto passable = [&](Vector2i center) {
        const auto pt = to_point(center);
        const auto min = Vector2(pt.intra().center_shifted() - Vector2i(q.body_size / 2u));
        return Search::is_passable(w, pt.chunk3(), Range2D{min, min + Vector2(q.body_size)}, Search::without_critters());
    };

    // A body flush against a blocker covers cells the blocker clears. The corner rays check those.
    const auto body_hidden = [&](Vector2i center) {
        const auto a = cell_of(center - Vector2i{Math::floor(half)});
        const auto b = cell_of(center + Vector2i{Math::ceil(half)} - Vector2i{1});
        for (auto y = a.y(); y <= b.y(); y++)
            for (auto x = a.x(); x <= b.x(); x++)
                if (const auto info = look({x, y}); !info.hidden && !info.blocked)
                    return false;
        return true;
    };

    Array<candidate> cands;
    arrayReserve(cands, 16);
    const auto against = Math::ceil(Math::max(half.x(), half.y())) + (float)ds;
    for (auto y = lo.y(); y < lo.y() + span.y(); y++)
        for (auto x = lo.x(); x < lo.x() + span.x(); x++)
        {
            const auto center = center_of({x, y});
            const auto off = center - P;
            if (off.dot() > R*R)
                continue;
            const auto info = look({x, y});
            const auto blocker_px = info.distance*(uint32_t)ds;
            if (!info.hidden || (float)blocker_px > against || !passable(center) || !body_hidden(center))
                continue;
            const auto a = Math::abs(off);
            const auto walk_dist = (float)Math::max(a.x(), a.y()) + (Math::sqrt(2.f) - 1) * (float)Math::min(a.x(), a.y());
            arrayAppend(cands, candidate{center, walk_dist, blocker_px});
        }
    std::sort(cands.begin(), cands.end(), [](const candidate& a, const candidate& b) {
        return a.walk_dist != b.walk_dist ? a.walk_dist < b.walk_dist : a.blocker_px < b.blocker_px;
    });

    cover_point_result ret;
    ret.candidates = (uint32_t)cands.size();

    const auto half_i = Vector2i(q.body_size / 2u);
    const auto min_size = (int)Math::min(q.body_size.x(), q.body_size.y());
    const auto step_px = Math::max(1, min_size/2);
    const auto max_push = (int)Math::ceil(against * Math::sqrt(2.f)) + min_size;

    const auto exposure_at = [&](Vector2i pos, Vector2i across) {
        exposure e{false, false};
        for (const auto corner : { Vector2i{-1, -1}, Vector2i{1, -1}, Vector2i{-1, 1}, Vector2i{1, 1} })
        {
            auto& side = Math::dot(corner, across) < 0 ? e.minus : e.plus;
            if (side)
                continue;
            const auto r = raycast(w, q.threat, to_point(pos + corner*half_i), 0, ray_pool, Search::without_critters());
            side = !r.has_result || r.success;
        }
        return e;
    };

    const auto place = [&](Vector2i start, Vector2i axis, Vector2i across) -> Optional<placed> {
        if (!passable(start))
            return {};
        // A blocker stops the body over at least the body's size along the axis, so steps of half
        // of it can't skip one.
        int lo_px = 0, hi_px = -1;
        for (auto p = step_px; p < max_push + step_px && hi_px < 0; p += step_px)
        {
            const auto pp = Math::min(p, max_push);
            if (passable(start + axis*pp))
                lo_px = pp;
            else
                hi_px = pp;
        }
        if (hi_px < 0)
            return {};
        while (hi_px - lo_px > 1)
        {
            const auto mid = (lo_px + hi_px) / 2;
            if (passable(start + axis*mid))
                lo_px = mid;
            else
                hi_px = mid;
        }
        const auto pos = start + axis*lo_px;
        return placed{pos, exposure_at(pos, across)};
    };

    for (const auto& cand : cands)
    {
        const auto v = T - cand.center;
        const auto axis = Math::abs(v.x()) >= Math::abs(v.y()) ? Vector2i{v.x() > 0 ? 1 : -1, 0}
                                                                : Vector2i{0, v.y() > 0 ? 1 : -1};
        const auto across = Math::abs(Vector2i{axis.y(), axis.x()});
        auto p = place(cand.center, axis, across);
        if (!p)
            continue;
        if (p->e.minus != p->e.plus)
        {
            // Candidates sit on cell centres, so cover only a little wider than the body needs a
            // shift away from the exposed side. Half a cell reaches the next candidate's range.
            const bool plus = p->e.plus;
            const auto dir = plus ? -across : across;
            const auto side_hidden = [plus](const Optional<placed>& x) {
                return x && !(plus ? x->e.plus : x->e.minus);
            };
            int lo_t = 0, hi_t = ds/2;
            if (auto x = place(cand.center + dir*hi_t, axis, across); side_hidden(x))
            {
                p = x;
                while (hi_t - lo_t > 1)
                {
                    const auto mid = (lo_t + hi_t) / 2;
                    if (auto y = place(cand.center + dir*mid, axis, across); side_hidden(y))
                    {
                        hi_t = mid;
                        p = y;
                    }
                    else
                        lo_t = mid;
                }
            }
        }
        if (p->e.minus || p->e.plus)
        {
            ret.rejected++;
            continue;
        }
        ret.pos = to_point(p->pos);
        ret.axis = axis;
        ret.found = true;
        break;
    }
    return ret;
}

} // namespace floormat
