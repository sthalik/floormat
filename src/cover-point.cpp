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
#include "compat/fwd/algorithm/sort.hpp"
#include <bit>
#include <cmath>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <mg/Range.h>

namespace floormat {

namespace {

constexpr float octant_step = 2.f * Math::Constants<float>::pi() / (float)Cover::octant_count;

struct octant_pair
{
    uint32_t lo, hi;
};

// The two octants whose bearings are on either side of v's.
octant_pair octants_around(Vector2 v)
{
    constexpr auto n = (int)Cover::octant_count;
    const auto k = (int)std::floor(std::atan2(v.y(), v.x()) / octant_step);
    const auto lo = (uint32_t)((k % n + n) % n);
    return { lo, (lo + 1) % (uint32_t)n };
}

// The octant whose bearing is nearest v's. A bearing halfway between octants k and k + 1 takes
// k + 1, mod Cover::octant_count, since floor(x + 1/2) rounds a half up.
uint32_t nearest_octant(Vector2 v)
{
    constexpr auto n = (int)Cover::octant_count;
    const auto k = (int)std::floor(std::atan2(v.y(), v.x()) / octant_step + .5f);
    return (uint32_t)((k % n + n) % n);
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

// arrayResize() alone reallocates to exactly count whenever count exceeds the capacity, so a scratch
// array whose size varies per call would allocate again on every new largest size.
template<typename T> void reserve_scratch(Array<T>& A, size_t count)
{
    if (arrayCapacity(A) < count)
        arrayReserve(A, count + count/2);
}

} // namespace

struct cover_point::Impl
{
    Array<uint8_t> known;
    Array<cell_info> cells;
    Array<candidate> cands;
    Array<Range2Di> rects;
    Array<slope_range> runs;
    Array<cover_spot> spots;
    Array<Range2Di> turned_rects;
    Array<Range2Di> slide_rects;
};

cover_point::cover_point()
{
    arrayReserve(impl->cands, 16);
}

cover_point::~cover_point() noexcept = default;

cover_point_result cover_point::find_cover_point(world& w, Grid::PoolRegistry& pools, Cover::Params params,
                                                 const cover_query& q, const Search::pred& pred,
                                                 bool list_spots)
{
    auto& [known, cells, cands, rects, runs, spots, turned_rects, slide_rects] = *impl;
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
    const auto dc = chunk_size_xy / (uint32_t)ds;
    const auto P = Vector3i(q.pos).xy(), T = Vector3i(q.threat).xy();
    const auto body_lo = Vector2i(q.body_size / 2u), body_hi = Vector2i(q.body_size) - body_lo;
    const auto R = (int)q.radius;
    const auto to_point = [&](Vector2i px) { return q.pos + (px - P); };

    // cell n spans px n*ds - half_tile up to the next one, so cells line up with chunk edges
    const auto cell_of = [ds](Vector2i px) {
        return Vector2i{floor_div(px.x() + half_tile<int>, ds), floor_div(px.y() + half_tile<int>, ds)};
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
    const auto count = (size_t)(span.x()*span.y());
    arrayClear(known);
    reserve_scratch(known, count);
    arrayResize(known, ValueInit, count);
    arrayClear(cells);
    reserve_scratch(cells, count);
    arrayResize(cells, NoInit, count);

    // dc is also a miss, so a threat past the cap reads as exposed
    const auto hides = [&](uint16_t d, float len) {
        return d != Cover::blocked && d < dc && (float)(d*(uint32_t)ds) < len;
    };

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
        const auto cell = g.get_cell_index_from_coord(pt.intra());
        // The cell's center sees the threat between two octants. Either one hiding it makes a
        // candidate, since the slope check below decides exactly.
        const auto k = octants_around(v);
        (void)g.ensure_octant(k.lo);
        (void)g.ensure_octant(k.hi);
        const auto d0 = g.distance(cell, k.lo), d1 = g.distance(cell, k.hi);
        const auto d = hides(d0, len) && hides(d1, len) ? Math::min(d0, d1) : hides(d1, len) ? d1 : d0;
        cells[i] = { hides(d, len), d == Cover::blocked, d };
        return cells[i];
    };

    // Octant k's distance at cell n, empty when n's chunk doesn't exist. Cells under a body near a
    // chunk edge lie in the next chunk, whose octants are filled lazily too.
    const auto distance_at = [&](Vector2i n, uint32_t k) -> Optional<uint16_t> {
        const auto pt = to_point(center_of(n));
        auto* c = w.at(pt.chunk3());
        if (!c)
            return {};
        auto g = cover_pool[*c];
        (void)g.ensure_octant(k);
        return g.distance(g.get_cell_index_from_coord(pt.intra()), k);
    };

    const auto cells_under = [&](Vector2i center) {
        return cell_span{cell_of(center - body_lo), cell_of(center + body_hi - Vector2i{1})};
    };

    const auto passable = [&](Vector2i center) {
        const auto pt = to_point(center);
        const auto min = Vector2(pt.intra().center_shifted() - body_lo);
        return Search::is_passable(w, pt.chunk3(), Range2D{min, min + Vector2(q.body_size)}, pred);
    };

    // A body flush against a blocker covers cells the blocker clears. The slope check covers those.
    const auto body_hidden = [&](Vector2i center) {
        const auto [a, b] = cells_under(center);
        for (auto y = a.y(); y <= b.y(); y++)
            for (auto x = a.x(); x <= b.x(); x++)
                if (const auto info = look({x, y}); !info.hidden && !info.blocked)
                    return false;
        return true;
    };

    // True when, in each of the q.octants octants centered on the one nearest the bearing from center
    // to the threat, the highest distance among the cells under a body centered on center passes
    // hides() at the distance from center to the threat. Blocked cells are skipped, as body_hidden
    // skips them. False when a cell's chunk doesn't exist, as look() reads such a cell as seen.
    const auto half = (int)q.octants/2;
    const auto hidden_in_octants = [&](Vector2i center) {
        constexpr auto n = (int)Cover::octant_count;
        const auto v = Vector2(T - center);
        const auto len = v.length();
        const auto m = (int)nearest_octant(v);
        const auto [a, b] = cells_under(center);
        for (auto j = -half; j <= half; j++)
        {
            const auto k = (uint32_t)((m + j + n) % n);
            // Stays 0, which hides, when every cell is blocked.
            uint16_t highest = 0;
            for (auto y = a.y(); y <= b.y(); y++)
                for (auto x = a.x(); x <= b.x(); x++)
                {
                    const auto d = distance_at({x, y}, k);
                    if (!d)
                        return false;
                    if (*d != Cover::blocked)
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
            const auto blocker_px = info.distance*(uint32_t)ds;
            if (!info.hidden || (float)blocker_px > against || !passable(center)
                || !(q.octants ? hidden_in_octants(center) : body_hidden(center)))
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

    cover_point_result ret;
    ret.candidates = (uint32_t)cands.size();
    if (cands.isEmpty())
        return ret;

    const auto min_size = (int)Math::min(q.body_size.x(), q.body_size.y());
    const auto step_px = Math::max(1, min_size/2);
    const auto max_push = (int)Math::ceil(against * Math::sqrt(2.f)) + min_size;
    const auto ext_lo = body_lo, ext_hi = body_hi - Vector2i{1};
    const auto reach = [&](Vector2i dir) {
        return Math::dot(Math::max(dir, Vector2i{0}), ext_hi) + Math::dot(Math::max(-dir, Vector2i{0}), ext_lo);
    };
    // The candidate nearest a spot may be filtered out, so a shift reaches two cells.
    const auto range = 2*ds;

    // Appends the rects that the box from qlo to qhi touches, relative to threat and doubled. The
    // rects a ray skips are left out the same way.
    const auto collect_rects = [&](Vector2i threat, Vector2i qlo, Vector2i qhi, Array<Range2Di>& out) {
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
                        arrayAppend(out, Range2Di{(pmin + origin - threat)*2 - Vector2i{1}, (pmax + origin - threat)*2 - Vector2i{1}});
                    return true;
                });
            }
    };

    arrayClear(rects);
    {
        // Every segment from the threat to a body stays in this box, so one search serves all candidates.
        const auto spread = Vector2i{range + 1} + Math::max(ext_lo, ext_hi);
        collect_rects(T, Math::min(T, cand_lo - spread) - Vector2i{1}, Math::max(T, cand_hi + spread) + Vector2i{1},
                      rects);
    }

    const auto body_at = [&](Vector2i pos, Vector2i axis, Vector2i across, Vector2i threat) {
        const auto e = Math::dot(threat - pos, axis), c = Math::dot(pos - threat, across);
        return square_box{ 2*(e - reach(axis)) - 1, 2*(e + reach(-axis)) + 1,
                           2*(c - reach(-across)) - 1, 2*(c + reach(across)) + 1 };
    };
    const auto slopes_of = [](const square_box& b) {
        return slope_range{ { b.a0, b.a0 > 0 ? b.d1 : b.d0 }, { b.a1, b.a1 > 0 ? b.d0 : b.d1 } };
    };

    // A ray aimed anywhere hits a rect when it touches a closed pixel square (bresenham_line::off_axis_at).
    // Only squares nearer than the body's near face surely come first. Open ranges let a grazing ray pass.
    const auto build_runs = [&](const square_box& body, Vector2i axis, Vector2i across, ArrayView<const Range2Di> from) {
        arrayClear(runs);
        for (const auto& r : from)
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
            // Runs that only touch merge too when a corner at the touch is nearer than the body: a
            // ray at that slope touches its pixel square at least a pixel before the body's.
            if (n && (s.lo < runs[n-1].hi || !(runs[n-1].hi < s.lo) && Math::min(runs[n-1].hi.d, s.lo.d) < body.d0))
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

    const auto axis_toward = [](Vector2i v) {
        return Math::abs(v.x()) >= Math::abs(v.y()) ? Vector2i{v.x() > 0 ? 1 : -1, 0}
                                                    : Vector2i{0, v.y() > 0 ? 1 : -1};
    };

    // cos and sin of j octants, at j - 1, for j from 1 to half
    Vector2 turns[4];
    for (auto j = 1; j <= half; j++)
        turns[j - 1] = { std::cos((float)j * octant_step), std::sin((float)j * octant_step) };

    // True when the threat turned about pos by j octants either way, for j from 1 to half and rounded
    // to whole px, sees no pixel of a body centered on pos and pushed along axis. Each turned threat
    // gets its own R-tree search, of the box from it to the body. Shifting the threat's rects instead
    // would need their box widened to hold every turned threat of every candidate. With 4 octants
    // either way and a far threat, that box spans many chunks, and build_runs goes through each rect
    // in it for every spot, the tests from the threat where it stands included.
    // A turned threat gets the threat's slope test along axis, where the blocker the body is flush
    // against lies in front of it. When the turned threat sees the body there, and the bigger
    // component of the way to it is along the other axis, the test runs again along that one, as the
    // loop picks an axis for the threat. Either finding the body hidden is enough, since each test is
    // exact for the rects nearer than the body's near face along its axis. The second alone would
    // leave out a wall the body is flush against, as it lies beside the body along that axis.
    const auto hidden_from_turned = [&](Vector2i pos, Vector2i axis) {
        const auto v = Vector2(T - pos);
        for (auto j = 0; j < half; j++)
            for (const auto s : { turns[j].y(), -turns[j].y() })
            {
                const auto c = turns[j].x();
                const auto t = pos + Vector2i(Math::round(Vector2{v.x()*c - v.y()*s, v.x()*s + v.y()*c}));
                arrayClear(turned_rects);
                collect_rects(t, Math::min(t, pos - ext_lo) - Vector2i{1}, Math::max(t, pos + ext_hi) + Vector2i{1},
                              turned_rects);
                const auto seen_along = [&](Vector2i ax) {
                    const auto ac = Math::abs(Vector2i{ax.y(), ax.x()});
                    const auto body = body_at(pos, ax, ac, t);
                    build_runs(body, ax, ac, turned_rects);
                    return !hidden(body);
                };
                const auto own = axis_toward(t - pos);
                if (seen_along(axis) && (own == axis || seen_along(own)))
                    return false;
            }
        return true;
    };

    enum class slide_step : uint8_t { hidden, seen, stopped, };
    const auto slide_test = [&](Vector2i pos, Vector2i axis, Vector2i across) {
        const auto body = body_at(pos, axis, across, T);
        build_runs(body, axis, across, slide_rects);
        if (!hidden(body) || !hidden_from_turned(pos, axis))
            return slide_step::seen;
        return passable(pos) && !passable(pos + axis) ? slide_step::hidden : slide_step::stopped;
    };

    // The slid bodies can lie past the box of rects, which holds only the candidates' area, so the
    // slide searches the box from the threat to the bodies of both sides.
    const auto slide_to_edge = [&](Vector2i pos, Vector2i axis, Vector2i across) {
        const auto m = (int)q.max_slide;
        arrayClear(slide_rects);
        collect_rects(T, Math::min(T, pos - ext_lo - across*m) - Vector2i{1},
                      Math::max(T, pos + ext_hi + across*m) + Vector2i{1}, slide_rects);
        bool live[2] = { true, true };
        int lo[2] = { 0, 0 };
        for (auto t = step_px; t <= m && (live[0] || live[1]); t += step_px)
            for (auto i = 0u; i < 2; i++)
            {
                if (!live[i])
                    continue;
                const auto dir = i ? -across : across;
                const auto r = slide_test(pos + dir*t, axis, across);
                if (r == slide_step::stopped)
                    live[i] = false;
                else if (r == slide_step::hidden)
                    lo[i] = t;
                else
                {
                    auto a = lo[i], b = t;
                    while (b - a > 1)
                    {
                        const auto mid = (a + b)/2;
                        if (slide_test(pos + dir*mid, axis, across) == slide_step::hidden)
                            a = mid;
                        else
                            b = mid;
                    }
                    return pos + dir*a;
                }
            }
        return pos;
    };

    for (const auto& cand : cands)
    {
        const auto axis = axis_toward(T - cand.center);
        const auto across = Math::abs(Vector2i{axis.y(), axis.x()});
        auto p = place(cand.center, axis);
        if (!p)
            continue;
        auto body = body_at(*p, axis, across, T);
        build_runs(body, axis, across, rects);
        auto seen = !hidden(body);
        if (seen)
        {
            if (const auto t = shift_to_cover(body))
                if (const auto x = place(cand.center + across * *t, axis))
                {
                    p = x;
                    body = body_at(*p, axis, across, T);
                    build_runs(body, axis, across, rects);
                }
            seen = !hidden(body);
        }
        if (seen || !hidden_from_turned(*p, axis))
        {
            if (!ret.found)
                ret.rejected++;
            continue;
        }
        if (q.max_slide)
            p = slide_to_edge(*p, axis, across);
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
            arrayAppend(spots, cover_spot{pt, axis});
    }
    if (list_spots)
        ret.spots = spots;
    return ret;
}

} // namespace floormat
