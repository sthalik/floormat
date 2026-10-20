#include "cover-point.hpp"
#include "grid-cover.hpp"
#include "grid-pool-registry.hpp"
#include "collision.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "search.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "RTree-search.hpp"
#include "compat/floor-divmod.hpp"
#include "compat/function2.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
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

// a/d from the threat, d > 0
struct slope
{
    int64_t a, d;
};

bool operator<(slope x, slope y) { return x.a*y.d < y.a*x.d; }

struct slope_range
{
    slope lo, hi;
};

// Pixel squares in doubled coordinates from the threat, so their half-pixel edges are odd integers.
struct square_box
{
    int64_t d0, d1, a0, a1;
};

int64_t floor_div(int64_t x, int64_t d)
{
    fm_debug_assert(d > 0);
    const auto q = x / d;
    return q - (x % d < 0);
}

} // namespace

cover_point_result find_cover_point(world& w, Grid::PoolRegistry& pools, Cover::Params params, const cover_query& q)
{
    fm_assert(q.pos.chunk3().z == q.threat.chunk3().z);
    auto& cover_pool = pools.cover_pool(params);
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

    // A body flush against a blocker covers cells the blocker clears. The slope check covers those.
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
    if (cands.isEmpty())
        return ret;

    const auto half_i = Vector2i(q.body_size / 2u);
    const auto min_size = (int)Math::min(q.body_size.x(), q.body_size.y());
    const auto step_px = Math::max(1, min_size/2);
    const auto max_push = (int)Math::ceil(against * Math::sqrt(2.f)) + min_size;
    const auto ext_lo = half_i, ext_hi = Vector2i(q.body_size) - half_i - Vector2i{1};
    const auto reach = [&](Vector2i dir) {
        return Math::dot(Math::max(dir, Vector2i{0}), ext_hi) + Math::dot(Math::max(-dir, Vector2i{0}), ext_lo);
    };
    // The candidate nearest a spot may be filtered out, so a shift reaches two cells.
    const auto range = 2*ds;

    // The rects a ray skips are left out the same way.
    Array<Range2Di> rects;
    {
        // Every segment from the threat to a body stays in this box, so one search serves all candidates.
        const auto spread = Vector2i{R + range + 1} + Math::max(ext_lo, ext_hi);
        const auto qlo = Math::min(T, P - spread) - Vector2i{1}, qhi = Math::max(T, P + spread) + Vector2i{1};
        const auto z = q.pos.chunk3().z;
        const auto chunk_of = [](int px) { return floor_div(px + half_tile<int>, chunk_size<int>); };
        constexpr auto min_pass = (uint64_t)pass_mode::shoot_through;
        // a rect overhangs into the next chunk at most
        for (auto cy = chunk_of(qlo.y()) - 1; cy <= chunk_of(qhi.y()) + 1; cy++)
            for (auto cx = chunk_of(qlo.x()) - 1; cx <= chunk_of(qhi.x()) + 1; cx++)
            {
                auto* c = w.at(chunk_coords_{(int16_t)cx, (int16_t)cy, z});
                if (!c)
                    continue;
                const auto origin = Vector2i{cx, cy}*chunk_size<int>;
                const auto clo = Vector2(qlo - origin), chi = Vector2(qhi - origin);
                c->rtree()->Search(clo.data(), chi.data(), [&](uint64_t data, const auto& r) {
                    const auto x = std::bit_cast<collision_data>(data);
                    const auto range = Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}};
                    const auto pmin = Vector2i(Math::ceil(range.min())), pmax = Vector2i(Math::ceil(range.max()));
                    if (x.pass < min_pass && (pmin < pmax).all()
                        && Search::without_critters()(*c, x, range) != path_search_continue::pass)
                        arrayAppend(rects, Range2Di{(pmin + origin - T)*2 - Vector2i{1}, (pmax + origin - T)*2 - Vector2i{1}});
                    return true;
                });
            }
    }

    const auto body_at = [&](Vector2i pos, Vector2i axis, Vector2i across) {
        const auto e = Math::dot(T - pos, axis), c = Math::dot(pos - T, across);
        return square_box{ 2*(e - reach(axis)) - 1, 2*(e + reach(-axis)) + 1,
                           2*(c - reach(-across)) - 1, 2*(c + reach(across)) + 1 };
    };
    const auto slopes_of = [](const square_box& b) {
        return slope_range{ { b.a0, b.a0 > 0 ? b.d1 : b.d0 }, { b.a1, b.a1 > 0 ? b.d0 : b.d1 } };
    };

    // A ray aimed anywhere hits a rect when it touches a closed pixel square (bresenham_line::off_axis_at).
    // Only squares nearer than the body's near face surely come first. Open ranges let a grazing ray pass.
    Array<slope_range> runs;
    const auto build_runs = [&](const square_box& body, Vector2i axis, Vector2i across) {
        arrayClear(runs);
        for (const auto& r : rects)
        {
            const auto da = (int64_t)Math::dot(-r.min(), axis), db = (int64_t)Math::dot(-r.max(), axis);
            // 1 is half a pixel, where the threat's own pixel ends
            const auto d0 = Math::max(Math::min(da, db), int64_t{1}), d1 = Math::min(Math::max(da, db), body.d0);
            if (d0 >= d1)
                continue;
            arrayAppend(runs, slopes_of({ d0, d1, Math::dot(r.min(), across), Math::dot(r.max(), across) }));
        }
        std::sort(runs.begin(), runs.end(), [](const slope_range& a, const slope_range& b) { return a.lo < b.lo; });
        size_t n = 0;
        for (const auto& s : runs)
        {
            if (n && s.lo < runs[n-1].hi)
                runs[n-1].hi = runs[n-1].hi < s.hi ? s.hi : runs[n-1].hi;
            else
                runs[n++] = s;
        }
        arrayResize(runs, NoInit, n);
    };

    const auto hidden = [&](const square_box& body) {
        if (body.d0 <= 1)
            return false;
        const auto s = slopes_of(body);
        for (const auto& run : runs)
            if (run.lo < s.lo && s.hi < run.hi)
                return true;
        return false;
    };

    const auto shift_to_cover = [&](const square_box& body) -> Optional<int> {
        Optional<int> best;
        for (const auto& run : runs)
        {
            auto t_lo = (int64_t)-range, t_hi = (int64_t)range;
            for (const auto a : { body.a0, body.a1 })
                for (const auto d : { body.d0, body.d1 })
                {
                    // run.lo < (a + 2t)/d < run.hi
                    t_lo = Math::max(t_lo, floor_div(run.lo.a*d - a*run.lo.d, 2*run.lo.d) + 1);
                    t_hi = Math::min(t_hi, -floor_div(a*run.hi.d - run.hi.a*d, 2*run.hi.d) - 1);
                }
            if (t_lo > t_hi)
                continue;
            const auto t = (int)(t_lo > 0 ? t_lo : t_hi < 0 ? t_hi : 0);
            if (!best || Math::abs(t) < Math::abs(*best))
                best = t;
        }
        return best;
    };

    const auto place = [&](Vector2i start, Vector2i axis) -> Optional<Vector2i> {
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
        return start + axis*lo_px;
    };

    for (const auto& cand : cands)
    {
        const auto v = T - cand.center;
        const auto axis = Math::abs(v.x()) >= Math::abs(v.y()) ? Vector2i{v.x() > 0 ? 1 : -1, 0}
                                                                : Vector2i{0, v.y() > 0 ? 1 : -1};
        const auto across = Math::abs(Vector2i{axis.y(), axis.x()});
        auto p = place(cand.center, axis);
        if (!p)
            continue;
        auto body = body_at(*p, axis, across);
        build_runs(body, axis, across);
        if (!hidden(body))
        {
            if (const auto t = shift_to_cover(body))
                if (const auto x = place(cand.center + across * *t, axis))
                {
                    p = x;
                    body = body_at(*p, axis, across);
                    build_runs(body, axis, across);
                }
            if (!hidden(body))
            {
                ret.rejected++;
                continue;
            }
        }
        ret.pos = to_point(*p);
        ret.axis = axis;
        ret.found = true;
        break;
    }
    return ret;
}

} // namespace floormat
