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
#include "RTree-search.hpp"
#include "compat/floor-divmod.hpp"
#include "compat/function2.hpp"
#include <algorithm>
#include <bit>
#include <cfloat>
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

struct ray_hit
{
    Range2D rect;
    bool open;
};

struct slope_range
{
    float lo, hi;
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

    // A body flush against a blocker covers cells the blocker clears. exposure_at checks those.
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
    const auto ext = Math::max(half_i - Vector2i{1}, Vector2i{0});
    const auto min_pass = (uint64_t)ray_pool.params().min_pass;

    Array<Range2D> rects;
    Array<slope_range> runs;

    const auto cast = [&](Vector2i px) -> ray_hit {
        const auto r = raycast(w, q.threat, to_point(px), 0, ray_pool, Search::without_critters());
        return { r.rect, !r.has_result || r.success };
    };

    // The rects a ray skips are left out the same way.
    const auto collect_rects = [&](Vector2i lo, Vector2i hi) {
        arrayClear(rects);
        const auto z = q.pos.chunk3().z;
        const auto chunk_of = [](int px) { return floor_div(px + half_tile<int>, chunk_size<int>); };
        // a rect overhangs into the next chunk at most
        for (auto cy = chunk_of(lo.y()) - 1; cy <= chunk_of(hi.y()) + 1; cy++)
            for (auto cx = chunk_of(lo.x()) - 1; cx <= chunk_of(hi.x()) + 1; cx++)
            {
                auto* c = w.at(chunk_coords_{(int16_t)cx, (int16_t)cy, z});
                if (!c)
                    continue;
                const auto origin = Vector2(Vector2i{cx, cy}*chunk_size<int>);
                const auto qlo = Vector2(lo) - origin, qhi = Vector2(hi) - origin;
                c->rtree()->Search(qlo.data(), qhi.data(), [&](uint64_t data, const auto& r) {
                    const auto x = std::bit_cast<collision_data>(data);
                    const auto range = Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}};
                    if (x.pass < min_pass && Search::without_critters()(*c, x, range) != path_search_continue::pass)
                        arrayAppend(rects, Range2D{range.min() + origin, range.max() + origin});
                    return true;
                });
            }
    };

    // Targets lie 1 px inside the body, since one on a collider's face reads blocked. Along the faces
    // the threat sees, a convex rect blocking the rays to both ends of a stretch blocks every ray between.
    const auto exposure_at = [&](Vector2i pos, Vector2i axis, Vector2i across) -> exposure {
        const auto ea = Math::dot(across, ext), ex = Math::dot(Math::abs(axis), ext);
        const auto mid = pos + axis*ex;
        const auto at_mid = cast(mid);
        if (at_mid.open)
            return { true, true };
        exposure e{false, false};
        for (const auto sign : { -1, 1 })
        {
            const auto dir = across*sign, corner = mid + dir*ea;
            const auto len = ea + (Math::dot(T - pos, dir) > ea ? 2*ex : 0);
            const auto at = [&](int s) { return s <= ea ? mid + dir*s : corner - axis*(s - ea); };
            const auto hidden = [&](this const auto& self, int a, int b, const ray_hit& ra, const ray_hit& rb) -> bool {
                if (ra.rect == rb.rect || b - a <= 1)
                    return true;
                const auto m = (a + b)/2;
                const auto rm = cast(at(m));
                return !rm.open && self(a, m, ra, rm) && self(m, b, rm, rb);
            };
            const auto end = cast(at(len));
            (sign < 0 ? e.minus : e.plus) = end.open || !hidden(0, len, at_mid, end);
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
        return placed{pos, exposure_at(pos, axis, across)};
    };

    // From the threat, a point's slope is its offset across over its distance along the axis. Shifting
    // the body by t adds t/d to a corner's slope, so the shifts keeping all four corners in one merged
    // run of rect slopes form a range.
    const auto shift_to_cover = [&](Vector2i pos, Vector2i axis, Vector2i across, int prefer) -> Optional<int> {
        const auto range = 2*ds;
        const auto spread = across*(Math::dot(across, ext) + range) + Math::abs(axis)*ext;
        collect_rects(Math::min(T, pos - spread), Math::max(T, pos + spread));

        const auto Tf = Vector2(T), ax = Vector2(axis), ac = Vector2(across);
        const auto body_far = Math::dot(Tf - Vector2(pos), ax) + (float)Math::dot(Math::abs(axis), ext);
        arrayClear(runs);
        for (const auto& r : rects)
        {
            // the squares of the pixels a ray counts, [min, max)
            const auto lo = Math::ceil(r.min()) - Vector2{.5f}, hi = Math::ceil(r.max()) - Vector2{.5f};
            const auto da = Math::dot(Tf - lo, ax), db = Math::dot(Tf - hi, ax);
            const auto d0 = Math::max(Math::min(da, db), 1.f), d1 = Math::max(da, db);
            if (d1 < d0 || d0 >= body_far)
                continue;
            const auto s0 = Math::dot(lo - Tf, ac), s1 = Math::dot(hi - Tf, ac);
            arrayAppend(runs, slope_range{ s0 / (s0 >= 0 ? d1 : d0), s1 / (s1 >= 0 ? d0 : d1) });
        }
        if (runs.isEmpty())
            return {};
        std::sort(runs.begin(), runs.end(), [](const slope_range& a, const slope_range& b) { return a.lo < b.lo; });

        Optional<int> best;
        const auto consider = [&](slope_range s) {
            auto t_lo = -FLT_MAX, t_hi = FLT_MAX;
            for (const auto corner : { Vector2i{-1, -1}, Vector2i{1, -1}, Vector2i{-1, 1}, Vector2i{1, 1} })
            {
                const auto c = Vector2(pos + corner*ext);
                const auto d = Math::dot(Tf - c, ax), a = Math::dot(c - Tf, ac);
                t_lo = Math::max(t_lo, s.lo*d - a);
                t_hi = Math::min(t_hi, s.hi*d - a);
            }
            const auto lo = (int)Math::ceil(Math::max(t_lo, -(float)range)),
                       hi = (int)Math::floor(Math::min(t_hi, (float)range));
            if (lo > hi)
                return;
            // 0 already read exposed
            int t;
            if (lo > 0)
                t = lo;
            else if (hi < 0)
                t = hi;
            else if (lo <= prefer && prefer <= hi)
                t = prefer;
            else if (lo <= -prefer && -prefer <= hi)
                t = -prefer;
            else
                return;
            if (!best || Math::abs(t) < Math::abs(*best))
                best = t;
        };
        auto cur = runs[0];
        for (const auto& s : arrayView(runs).exceptPrefix(1))
        {
            if (s.lo <= cur.hi)
                cur.hi = Math::max(cur.hi, s.hi);
            else
            {
                consider(cur);
                cur = s;
            }
        }
        consider(cur);
        return best;
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
        if (p->e.minus || p->e.plus)
        {
            // The candidate nearest a spot may be filtered out, so a shift reaches two cells.
            const auto prefer = p->e.plus && !p->e.minus ? -1 : 1;
            if (const auto t = shift_to_cover(p->pos, axis, across, prefer))
                if (auto x = place(cand.center + across * *t, axis, across))
                    p = x;
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
