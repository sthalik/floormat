#include "cover-point.hpp"
#include "grid-cover.hpp"
#include "grid-registry.hpp"
#include "collision.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "search.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "RTree-search.hpp"
#include "compat/floor-divmod.hpp"
#include "compat/function2.hpp"
#include "compat/fwd/algorithm/sort.hpp"
#include <bit>
#include <cmath>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <mg/Range.h>

namespace floormat::Cover::Point {

namespace {

constexpr float octant_step = 2.f * Math::Constants<float>::pi() / (float)Grid::Cover::octant_count;

struct octant_pair
{
    u32 lo, hi;
};

// The two octants whose bearings are on either side of v's.
octant_pair octants_around(Vector2 v)
{
    constexpr auto n = (int)Grid::Cover::octant_count;
    const auto k = (int)std::floor(std::atan2(v.y(), v.x()) / octant_step);
    const auto lo = (u32)((k % n + n) % n);
    return { lo, (lo + 1) % (u32)n };
}

// Cells first to last, both included, on both axes.
struct cell_span
{
    Vector2i first, last;
};

struct candidate
{
    Vector2i center;
    float walk_dist;
    u32 blocker_px;
};

struct cell_info
{
    bool hidden, blocked;
    u16 distance;
};

// a/d from the threat, d > 0
struct slope
{
    i64 a, d;
};

bool operator<(slope x, slope y) { return x.a*y.d < y.a*x.d; }

struct slope_range
{
    slope lo, hi;
};

// Pixel squares in doubled coordinates from the threat, so their half-pixel edges are odd integers.
struct square_box
{
    i64 d0, d1, a0, a1;
};

i64 floor_div(i64 x, i64 d)
{
    fm_debug_assert(d > 0);
    const auto q = x / d;
    return q - (x % d < 0);
}

// arrayResize() alone reallocates to exactly count whenever count exceeds the capacity, so a scratch
// array whose size varies per call would allocate again on every new largest size.
template<typename T> void reserve_scratch(Array<T>& A, usize count)
{
    if (arrayCapacity(A) < count)
        arrayReserve(A, count + count/2);
}

slope_range slopes_of(const square_box& b)
{
    return { { b.a0, b.a0 > 0 ? b.d1 : b.d0 }, { b.a1, b.a1 > 0 ? b.d0 : b.d1 } };
}

enum class end_step : u8 { hidden, seen, stopped, };

// The exact tests of a body of q.body_size against the R-tree rects between it and q.threat. Positions are in
// whole px, as Vector3i(point).xy() gives them.
struct spot_tests
{
    spot_tests(world& w, const query& q, const Search::pred& pred, Array<slope_range>& runs,
               Array<Range2Di>& turned_rects, Array<Range2Di>& end_rects);

    point to_point(Vector2i px) const { return q_pos + (px - P); }
    bool passable(Vector2i center) const;
    int reach(Vector2i dir) const;
    void collect_rects(Vector2i threat, Vector2i qlo, Vector2i qhi, Array<Range2Di>& out) const;
    square_box body_at(Vector2i pos, Vector2i axis, Vector2i across, Vector2i threat) const;
    void build_runs(const square_box& body, Vector2i axis, Vector2i across, ArrayView<const Range2Di> from);
    bool hidden(const square_box& body) const;
    bool hidden_from(Vector2i threat, Vector2i pos, Vector2i axis, Array<Range2Di>& rects);
    bool hidden_from_turned(Vector2i pos, Vector2i axis);
    end_step end_test(Vector2i pos, Vector2i axis, Vector2i across);
    Vector2i move_spot_to_cover_end(Vector2i pos, Vector2i axis, Vector2i across);

    world& w;
    const Search::pred& pred;
    Array<slope_range>& runs;
    Array<Range2Di>& turned_rects;
    Array<Range2Di>& end_rects;
    point q_pos;
    Vector2i P, T;
    Vector2ui body_size;
    Vector2i body_lo, body_hi, ext_lo, ext_hi;
    int half, step_px;
    u32 max_cover_end_dist;
    // cos and sin of j octants, at j - 1, for j from 1 to half
    Vector2 turns[4];
};

spot_tests::spot_tests(world& w, const query& q, const Search::pred& pred, Array<slope_range>& runs,
                       Array<Range2Di>& turned_rects, Array<Range2Di>& end_rects) :
    w{w}, pred{pred}, runs{runs}, turned_rects{turned_rects}, end_rects{end_rects}, q_pos{q.pos},
    P{Vector3i(q.pos).xy()}, T{Vector3i(q.threat).xy()}, body_size{q.body_size},
    body_lo{Vector2i(q.body_size / 2u)}, body_hi{Vector2i(q.body_size) - body_lo},
    ext_lo{body_lo}, ext_hi{body_hi - Vector2i{1}}, half{(int)(q.octants/2u)},
    step_px{Math::max(1, (int)Math::min(q.body_size.x(), q.body_size.y())/2)},
    max_cover_end_dist{q.max_cover_end_dist}
{
    for (auto j = 1; j <= half; j++)
        turns[j - 1] = { std::cos((float)j * octant_step), std::sin((float)j * octant_step) };
}

bool spot_tests::passable(Vector2i center) const
{
    const auto pt = to_point(center);
    const auto min = Vector2(pt.intra().center_shifted() - body_lo);
    return Search::is_passable(w, pt.chunk3(), Range2D{min, min + Vector2(body_size)}, pred);
}

int spot_tests::reach(Vector2i dir) const
{
    return Math::dot(Math::max(dir, Vector2i{0}), ext_hi) + Math::dot(Math::max(-dir, Vector2i{0}), ext_lo);
}

// Appends the rects that the box from qlo to qhi touches, relative to threat and doubled. The
// rects a ray skips are left out the same way.
void spot_tests::collect_rects(Vector2i threat, Vector2i qlo, Vector2i qhi, Array<Range2Di>& out) const
{
    const auto z = q_pos.chunk3().z;
    const auto chunk_of = [](int px) { return floormat::floor_div(px + half_tile<int>, chunk_size<int>); };
    constexpr auto min_pass = (u64)pass_mode::shoot_through;
    // a rect overhangs into the next chunk at most
    for (auto cy = chunk_of(qlo.y()) - 1; cy <= chunk_of(qhi.y()) + 1; cy++)
        for (auto cx = chunk_of(qlo.x()) - 1; cx <= chunk_of(qhi.x()) + 1; cx++)
        {
            auto* c = w.at(chunk_coords_{(i16)cx, (i16)cy, z});
            if (!c)
                continue;
            const auto origin = Vector2i{cx, cy}*chunk_size<int>;
            const auto clo = Vector2(qlo - origin), chi = Vector2(qhi - origin);
            c->rtree()->Search(clo.data(), chi.data(), [&](u64 data, const auto& r) {
                const auto x = std::bit_cast<collision_data>(data);
                const auto range = Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}};
                const auto pmin = Vector2i(Math::ceil(range.min())), pmax = Vector2i(Math::ceil(range.max()));
                if (x.pass < min_pass && (pmin < pmax).all()
                    && Search::without_critters()(*c, x, range) != path_search_continue::pass)
                    arrayAppend(out, Range2Di{(pmin + origin - threat)*2 - Vector2i{1}, (pmax + origin - threat)*2 - Vector2i{1}});
                return true;
            });
        }
}

square_box spot_tests::body_at(Vector2i pos, Vector2i axis, Vector2i across, Vector2i threat) const
{
    const auto e = Math::dot(threat - pos, axis), c = Math::dot(pos - threat, across);
    return square_box{ 2*(e - reach(axis)) - 1, 2*(e + reach(-axis)) + 1,
                       2*(c - reach(-across)) - 1, 2*(c + reach(across)) + 1 };
}

// A ray aimed anywhere hits a rect when it touches a closed pixel square (bresenham_line::orthogonal_at).
// Only squares nearer than the body's near face surely come first. Open ranges let a grazing ray pass.
void spot_tests::build_runs(const square_box& body, Vector2i axis, Vector2i across, ArrayView<const Range2Di> from)
{
    arrayClear(runs);
    for (const auto& r : from)
    {
        const auto da = (i64)Math::dot(-r.min(), axis), db = (i64)Math::dot(-r.max(), axis);
        // 1 is half a pixel, where the threat's own pixel ends
        const auto d0 = Math::max(Math::min(da, db), i64{1}), d1 = Math::min(Math::max(da, db), body.d0);
        if (d0 >= d1)
            continue;
        arrayAppend(runs, slopes_of({ d0, d1, Math::dot(r.min(), across), Math::dot(r.max(), across) }));
    }
    std::sort(runs.begin(), runs.end(), [](const slope_range& a, const slope_range& b) { return a.lo < b.lo; });
    usize n = 0;
    for (const auto& s : runs)
    {
        // Runs that only touch merge too when a corner at the touch is nearer than the body: a
        // ray at that slope touches its pixel square at least a pixel before the body's.
        if (n && (s.lo < runs[n-1].hi || !(runs[n-1].hi < s.lo) && Math::min(runs[n-1].hi.d, s.lo.d) < body.d0))
            runs[n-1].hi = runs[n-1].hi < s.hi ? s.hi : runs[n-1].hi;
        else
            runs[n++] = s;
    }
    arrayResize(runs, NoInit, n);
}

bool spot_tests::hidden(const square_box& body) const
{
    if (body.d0 <= 1)
        return false;
    const auto s = slopes_of(body);
    for (const auto& run : runs)
        if (run.lo < s.lo && s.hi < run.hi)
            return true;
    return false;
}

// True when threat sees no pixel of a body centered on pos, judged along axis and along the axis toward threat.
// rects is filled for the box spanning both.
bool spot_tests::hidden_from(Vector2i threat, Vector2i pos, Vector2i axis, Array<Range2Di>& rects)
{
    arrayClear(rects);
    collect_rects(threat, Math::min(threat, pos - ext_lo) - Vector2i{1}, Math::max(threat, pos + ext_hi) + Vector2i{1},
                  rects);
    const auto seen_along = [&](Vector2i ax) {
        const auto ac = across_of(ax);
        const auto body = body_at(pos, ax, ac, threat);
        build_runs(body, ax, ac, rects);
        return !hidden(body);
    };
    const auto own = axis_toward(threat - pos);
    return !seen_along(axis) || own != axis && !seen_along(own);
}

// True when the threat turned about pos by up to q.octants/2 octants either way, rounded to whole
// px, sees no pixel of a body centered on pos and pushed along axis. Each turned threat gets its
// own R-tree search, as shifting the rects would need a box spanning every candidate's turns.
bool spot_tests::hidden_from_turned(Vector2i pos, Vector2i axis)
{
    const auto v = Vector2(T - pos);
    for (auto j = 0; j < half; j++)
        for (const auto s : { turns[j].y(), -turns[j].y() })
        {
            const auto c = turns[j].x();
            const auto t = pos + Vector2i(Math::round(Vector2{v.x()*c - v.y()*s, v.x()*s + v.y()*c}));
            if (!hidden_from(t, pos, axis, turned_rects))
                return false;
        }
    return true;
}

end_step spot_tests::end_test(Vector2i pos, Vector2i axis, Vector2i across)
{
    const auto body = body_at(pos, axis, across, T);
    build_runs(body, axis, across, end_rects);
    if (!hidden(body) || !hidden_from_turned(pos, axis))
        return end_step::seen;
    return passable(pos) && !passable(pos + axis) ? end_step::hidden : end_step::stopped;
}

// The moved bodies can lie past the box of rects, which holds only the candidates' area, so this
// searches its own box from the threat to the bodies on both sides.
Vector2i spot_tests::move_spot_to_cover_end(Vector2i pos, Vector2i axis, Vector2i across)
{
    const auto m = (int)max_cover_end_dist;
    arrayClear(end_rects);
    collect_rects(T, Math::min(T, pos - ext_lo - across*m) - Vector2i{1},
                  Math::max(T, pos + ext_hi + across*m) + Vector2i{1}, end_rects);
    bool live[2] = { true, true };
    int lo[2] = { 0, 0 };
    for (auto t = step_px; t <= m && (live[0] || live[1]); t += step_px)
        for (auto i = 0u; i < 2; i++)
        {
            if (!live[i])
                continue;
            const auto dir = i ? -across : across;
            const auto r = end_test(pos + dir*t, axis, across);
            if (r == end_step::stopped)
                live[i] = false;
            else if (r == end_step::hidden)
                lo[i] = t;
            else
            {
                auto a = lo[i], b = t;
                while (b - a > 1)
                {
                    const auto mid = (a + b)/2;
                    if (end_test(pos + dir*mid, axis, across) == end_step::hidden)
                        a = mid;
                    else
                        b = mid;
                }
                return pos + dir*a;
            }
        }
    return pos;
}

} // namespace

Vector2i axis_toward(Vector2i v)
{
    return Math::abs(v.x()) >= Math::abs(v.y()) ? Vector2i{v.x() > 0 ? 1 : -1, 0}
                                                : Vector2i{0, v.y() > 0 ? 1 : -1};
}

Vector2i across_of(Vector2i axis) { return Math::abs(Vector2i{axis.y(), axis.x()}); }

// A bearing halfway between octants k and k + 1 takes k + 1, mod Grid::Cover::octant_count, since
// floor(x + 1/2) rounds a half up.
u32 nearest_octant(Vector2 v)
{
    constexpr auto n = (int)Grid::Cover::octant_count;
    const auto k = (int)std::floor(std::atan2(v.y(), v.x()) / octant_step + .5f);
    return (u32)((k % n + n) % n);
}

void nearest_tiles(u32 radius, Array<Vector2i>& out)
{
    arrayClear(out);
    const auto n = (int)radius/tile_size_xy + 1;
    reserve_scratch(out, (usize)((2*n + 1)*(2*n + 1)));
    for (auto y = -n; y <= n; y++)
        for (auto x = -n; x <= n; x++)
        {
            const auto gap = Math::max(Math::abs(Vector2i{x, y}*tile_size_xy) - Vector2i{half_tile<int>}, Vector2i{});
            if (gap.dot() <= (int)(radius*radius))
                arrayAppend(out, Vector2i{x, y});
        }
    std::sort(out.begin(), out.end(), [](Vector2i a, Vector2i b) {
        if (a.dot() != b.dot())
            return a.dot() < b.dot();
        return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x();
    });
}

struct finder::Impl
{
    Array<u8> known;
    Array<cell_info> cells;
    Array<candidate> cands;
    Array<Range2Di> rects;
    Array<slope_range> runs;
    Array<spot> spots;
    Array<Range2Di> turned_rects;
    Array<Range2Di> end_rects;
};

finder::finder()
{
    arrayReserve(impl->cands, 16);
    arrayReserve(impl->rects, 16);
    arrayReserve(impl->runs, 16);
    arrayReserve(impl->turned_rects, 16);
    arrayReserve(impl->end_rects, 16);
}

finder::~finder() noexcept = default;

result finder::find(world& w, Grid::GridRegistry& pools, Grid::Cover::Params params,
                    const query& q, const Search::pred& pred, bool list_spots)
{
    auto& [known, cells, cands, rects, runs, spots, turned_rects, end_rects] = *impl;
    fm_assert(q.pos.chunk3().z == q.threat.chunk3().z);
    // Odd, so the octants checked lie evenly on both sides of the nearest one.
    fm_assert(q.octants == 0 || q.octants % 2 == 1 && q.octants <= 9);
    if (list_spots)
    {
        arrayClear(spots);
        arrayReserve(spots, 16);
    }
    auto& cover_pool = pools.cover_pool(params);
    cover_pool.maybe_mark_stale_all(w);

    const auto ds = (int)cover_pool.params().div_size;
    const auto dc = chunk_size_xy / (u32)ds;
    const auto R = (int)q.radius;
    spot_tests X{w, q, pred, runs, turned_rects, end_rects};
    const auto &P = X.P, &T = X.T, &body_lo = X.body_lo, &body_hi = X.body_hi;
    const auto to_point = [&](Vector2i px) { return X.to_point(px); };

    // cell n spans px n*ds - half_tile up to the next one, so cells line up with chunk edges
    const auto cell_of = [ds](Vector2i px) {
        return Vector2i{floormat::floor_div(px.x() + half_tile<int>, ds), floormat::floor_div(px.y() + half_tile<int>, ds)};
    };
    const auto center_of = [ds](Vector2i n) { return n*ds + Vector2i{ds/2 - half_tile<int>}; };

    // Where candidate centers can lie, both ends included.
    auto cand_lo = P - Vector2i{R}, cand_hi = P + Vector2i{R};
    if (q.area)
    {
        cand_lo = Math::max(cand_lo, P + q.area->min());
        cand_hi = Math::min(cand_hi, P + q.area->max() - Vector2i{1});
        if (!(cand_lo <= cand_hi).all())
            return {};
    }
    const auto in_area = [&](Vector2i center) {
        return !q.area || (center - P >= q.area->min()).all() && (center - P < q.area->max()).all();
    };

    const auto lo = cell_of(cand_lo - body_hi), span = cell_of(cand_hi + body_hi) - lo + Vector2i{1};
    const auto count = (usize)(span.x()*span.y());
    arrayClear(known);
    reserve_scratch(known, count);
    arrayResize(known, ValueInit, count);
    arrayClear(cells);
    reserve_scratch(cells, count);
    arrayResize(cells, NoInit, count);

    // dc is also a miss, so a threat past the cap reads as exposed
    const auto hides = [&](u16 d, float len) {
        return d != Grid::Cover::blocked && d < dc && (float)(d*(u32)ds) < len;
    };

    const auto fills0 = cover_pool.octant_fills();
    bool out_of_fills = false;
    const auto may_fill = [&] {
        out_of_fills = out_of_fills || cover_pool.octant_fills() - fills0 >= q.max_fills;
        return !out_of_fills;
    };
    // False, with g unchanged, when building g or filling octant k would pass q.max_fills.
    const auto ensure = [&](Grid::Cover::Grid& g, u32 k) {
        if (g.is_stale())
        {
            if (!may_fill())
                return false;
            g.build_if_stale();
        }
        if (!(g.built_octants() & 1u << k))
        {
            if (!may_fill())
                return false;
            (void)g.ensure_octant(k);
        }
        return true;
    };

    const auto look = [&](Vector2i n) -> cell_info {
        const auto i = (usize)((n.y() - lo.y())*span.x() + n.x() - lo.x());
        if (known[i])
            return cells[i];
        known[i] = true;
        cells[i] = { false, false, Grid::Cover::blocked };
        const auto center = center_of(n);
        const auto pt = to_point(center);
        auto* c = w.at(pt.chunk3());
        const auto v = Vector2(T - center);
        const auto len = v.length();
        if (!c || len < 1)
            return cells[i];
        auto g = cover_pool[*c];
        // The cell's center sees the threat between two octants. Either one hiding it makes a
        // candidate, since the slope check below decides exactly.
        const auto k = octants_around(v);
        if (!ensure(g, k.lo) || !ensure(g, k.hi))
        {
            known[i] = false;
            return cells[i];
        }
        const auto cell = g.get_cell_index_from_coord(pt.intra());
        const auto d0 = g.distance(cell, k.lo), d1 = g.distance(cell, k.hi);
        const auto d = hides(d0, len) && hides(d1, len) ? Math::min(d0, d1) : hides(d1, len) ? d1 : d0;
        cells[i] = { hides(d, len), d == Grid::Cover::blocked, d };
        return cells[i];
    };

    // Octant k's distance at cell n, empty when n's chunk doesn't exist or out of fills. Cells under
    // a body near a chunk edge lie in the next chunk, whose octants are filled lazily too.
    const auto distance_at = [&](Vector2i n, u32 k) -> Optional<u16> {
        const auto pt = to_point(center_of(n));
        auto* c = w.at(pt.chunk3());
        if (!c)
            return {};
        auto g = cover_pool[*c];
        if (!ensure(g, k))
            return {};
        return g.distance(g.get_cell_index_from_coord(pt.intra()), k);
    };

    const auto cells_under = [&](Vector2i center) {
        return cell_span{cell_of(center - body_lo), cell_of(center + body_hi - Vector2i{1})};
    };

    const auto passable = [&](Vector2i center) { return X.passable(center); };

    // A body flush against a blocker covers cells the blocker clears. The slope check covers those.
    const auto body_hidden = [&](Vector2i center) {
        const auto [a, b] = cells_under(center);
        for (auto y = a.y(); y <= b.y(); y++)
            for (auto x = a.x(); x <= b.x(); x++)
                if (const auto info = look({x, y}); !info.hidden && !info.blocked)
                    return false;
        return true;
    };

    // True when, in each of the q.octants octants centered on the one nearest the bearing to the
    // threat, the highest distance among the cells under a body at center passes hides() at the
    // distance to the threat. False when a cell's chunk doesn't exist, as look() reads it as seen.
    const auto half = (int)(q.octants/2u);
    const auto hidden_in_octants = [&](Vector2i center) {
        constexpr auto n = (int)Grid::Cover::octant_count;
        const auto v = Vector2(T - center);
        const auto len = v.length();
        const auto m = (int)nearest_octant(v);
        const auto [a, b] = cells_under(center);
        for (auto j = -half; j <= half; j++)
        {
            const auto k = (u32)((m + j + n) % n);
            // Stays 0, which hides, when every cell is blocked.
            u16 highest = 0;
            for (auto y = a.y(); y <= b.y(); y++)
                for (auto x = a.x(); x <= b.x(); x++)
                {
                    const auto d = distance_at({x, y}, k);
                    if (!d)
                        return false;
                    if (*d != Grid::Cover::blocked)
                        highest = Math::max(highest, *d);
                }
            if (!hides(highest, len))
                return false;
        }
        return true;
    };

    arrayClear(cands);
    const auto against = (float)(body_hi.max() + ds);
    for (auto y = lo.y(); y < lo.y() + span.y(); y++)
        for (auto x = lo.x(); x < lo.x() + span.x(); x++)
        {
            const auto center = center_of({x, y});
            const auto off = center - P;
            if (off.dot() > R*R || !in_area(center))
                continue;
            // With q.octants, look() still reads the center's two bracketing octants, for blocker_px.
            // Its hidden is then also implied by hidden_in_octants(), unless the center is blocked.
            const auto info = look({x, y});
            const auto blocker_px = info.distance*(u32)ds;
            const auto ok = info.hidden && (float)blocker_px <= against && passable(center)
                            && (q.octants ? hidden_in_octants(center) : body_hidden(center));
            if (out_of_fills)
            {
                result ret;
                ret.fills = cover_pool.octant_fills() - fills0;
                ret.out_of_fills = true;
                return ret;
            }
            if (!ok)
                continue;
            const auto a = Math::abs(off);
            const auto walk_dist = (float)Math::max(a.x(), a.y()) + (Math::sqrt(2.f) - 1) * (float)Math::min(a.x(), a.y());
            arrayAppend(cands, candidate{center, walk_dist, blocker_px});
        }
    // Cells mirrored about the npc's row or column can tie on both keys. std::sort leaves the
    // order of equal elements to the standard library, so the position breaks the tie and every
    // library picks the same cell.
    std::sort(cands.begin(), cands.end(), [](const candidate& a, const candidate& b) {
        if (a.walk_dist != b.walk_dist)
            return a.walk_dist < b.walk_dist;
        if (a.blocker_px != b.blocker_px)
            return a.blocker_px < b.blocker_px;
        return a.center.y() != b.center.y() ? a.center.y() < b.center.y() : a.center.x() < b.center.x();
    });

    result ret;
    ret.candidates = (u32)cands.size();
    ret.fills = cover_pool.octant_fills() - fills0;
    if (cands.isEmpty())
        return ret;

    const auto min_size = (int)Math::min(q.body_size.x(), q.body_size.y());
    const auto step_px = X.step_px;
    const auto max_push = (int)Math::ceil(against * Math::sqrt(2.f)) + min_size;
    // The candidate nearest a spot may be filtered out, so a shift reaches two cells.
    const auto range = 2*ds;

    arrayClear(rects);
    {
        // Every segment from the threat to a body stays in this box, so one search serves all candidates.
        const auto spread = Vector2i{range + 1} + Math::max(X.ext_lo, X.ext_hi);
        X.collect_rects(T, Math::min(T, cand_lo - spread) - Vector2i{1}, Math::max(T, cand_hi + spread) + Vector2i{1},
                        rects);
    }

    const auto shift_to_cover = [&](const square_box& body) -> Optional<int> {
        Optional<int> best;
        for (const auto& run : runs)
        {
            auto t_lo = (i64)-range, t_hi = (i64)range;
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
        const auto axis = axis_toward(T - cand.center);
        const auto across = across_of(axis);
        auto p = place(cand.center, axis);
        if (!p)
            continue;
        auto body = X.body_at(*p, axis, across, T);
        X.build_runs(body, axis, across, rects);
        auto seen = !X.hidden(body);
        if (seen)
        {
            if (const auto t = shift_to_cover(body))
                if (const auto x = place(cand.center + across * *t, axis))
                {
                    p = x;
                    body = X.body_at(*p, axis, across, T);
                    X.build_runs(body, axis, across, rects);
                }
            seen = !X.hidden(body);
        }
        if (seen || !X.hidden_from_turned(*p, axis))
        {
            if (!ret.found)
                ret.rejected++;
            continue;
        }
        if (q.max_cover_end_dist)
            p = X.move_spot_to_cover_end(*p, axis, across);
        const auto pt = to_point(*p);
        if (!ret.found)
        {
            ret.pos = pt;
            ret.axis = axis;
            ret.found = true;
        }
        if (!list_spots)
            break;
        auto i = 0u;
        while (i < spots.size() && spots[i].pos != pt)
            i++;
        if (i == spots.size())
            arrayAppend(spots, spot{pt, axis});
    }
    if (list_spots)
        ret.spots = spots;
    return ret;
}

bool finder::hidden_at(world& w, point threat, const spot& s, Vector2ui body_size)
{
    fm_assert(s.pos.chunk3().z == threat.chunk3().z);
    auto& I = *impl;
    spot_tests X{w, {s.pos, body_size, threat, 0}, Search::never_continue(), I.runs, I.turned_rects, I.end_rects};
    return X.hidden_from(X.T, X.P, s.axis, I.rects);
}

point finder::move_to_cover_end(world& w, const query& q, Vector2i axis, const Search::pred& pred,
                               ArrayView<const point> also_from)
{
    fm_assert(q.pos.chunk3().z == q.threat.chunk3().z);
    auto& I = *impl;
    spot_tests X{w, q, pred, I.runs, I.turned_rects, I.end_rects};
    const auto across = across_of(axis);
    const auto end = X.move_spot_to_cover_end(X.P, axis, across);
    const auto hidden_from_all = [&](Vector2i pos) {
        for (const auto& t : also_from)
            if (!X.hidden_from(Vector3i(t).xy(), pos, axis, I.rects))
                return false;
        return true;
    };
    if (end == X.P || hidden_from_all(end))
        return X.to_point(end);
    // Assumes the stretch nearer q.pos stays hidden, as the move does.
    const auto len = Math::abs(end.x() - X.P.x()) + Math::abs(end.y() - X.P.y());
    const auto dir = (end - X.P)/len;
    auto a = 0, b = len;
    while (b - a > 1)
    {
        const auto mid = (a + b)/2;
        if (X.end_test(X.P + dir*mid, axis, across) == end_step::hidden && hidden_from_all(X.P + dir*mid))
            a = mid;
        else
            b = mid;
    }
    return X.to_point(X.P + dir*a);
}

} // namespace floormat::Cover::Point
