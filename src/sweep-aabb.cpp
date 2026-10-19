#include "sweep-aabb.hpp"
#include "compat/limits.hpp"
#include "compat/function2.hpp"
#include "collision.hpp"
#include "chunk.hpp"
#include "world.hpp"
#include "search.hpp"
#include "rect-intersects.hpp"
#include "tile-constants.hpp"
#include "raycast-line.hpp"
#include "raycast-loop.hpp"
#include "grid-pass.hpp"
#include "RTree-search.hpp"
#include <bit>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

auto axis_overlap(float a_lo, float a_hi, float v, float b_lo, float b_hi) {
    constexpr auto minval = limits<float>::min, maxval = limits<float>::max;
    struct { float t_lo, t_hi; } r;

    if (v == 0.0f) {
        // No motion on this axis. Either the projections already overlap (and this
        // axis can't constrain the TOI), or they never will.
        if (a_hi <= b_lo || a_lo >= b_hi) {
            r.t_lo = maxval;
            r.t_hi = minval;
        } else {
            r.t_lo = minval;
            r.t_hi = maxval;
        }
        return r;
    }
    const float t1 = (b_lo - a_hi) / v;
    const float t2 = (b_hi - a_lo) / v;
    r.t_lo = Math::min(t1, t2);
    r.t_hi = Math::max(t1, t2);
    return r;
};

} // namespace

sweep_result sweep_aabb_vs_aabb(Range2D start, Vector2 displacement, Range2D obstacle)
{
    // Pure-translation assumption: derive displacement from the corner deltas.
    // (start.max - start.min) and (end.max - end.min) must be equal — i.e., bbox size unchanged.
    const auto x = axis_overlap(start.min().x(), start.max().x(), displacement.x(),
                                obstacle.min().x(), obstacle.max().x());
    const auto y = axis_overlap(start.min().y(), start.max().y(), displacement.y(),
                                obstacle.min().y(), obstacle.max().y());

    const float t_enter = Math::max(x.t_lo, y.t_lo);
    const float t_exit  = Math::min(x.t_hi, y.t_hi);

    if (t_enter >= t_exit || t_enter >= 1.0f || t_exit <= 0.0f)
        return { false, /*0.0f*/ };
    else
        return { true, /*Math::max(0.0f, t_enter)*/ };
}

sweep_result find_swept_collider(chunk& c, Range2D start, Vector2 displacement, const Search::pred& p)
{
    const auto end_min = start.min() + displacement;
    const auto end_max = start.max() + displacement;
    const Vector2 search_min{
        Math::min(start.min().x(), end_min.x()),
        Math::min(start.min().y(), end_min.y()),
    };
    const Vector2 search_max{
        Math::max(start.max().x(), end_max.x()),
        Math::max(start.max().y(), end_max.y()),
    };

    const auto self_coord = c.coord();

    sweep_result res = { .has_collider = false, /*.pos = limits<float>::max*/ };

    auto cb = [&](chunk& self, collision_data data, Range2D r) {
        if (p(self, data, r) == path_search_continue::pass)
            return path_search_continue::pass;
        // Search::detail::is_passable_ shifts the search bbox into self's frame before firing
        // the pred, so r is in self-local coords; lift back into c's frame.
        const auto sc = self.coord();
        const Vector2 off{
            (float)(sc.x - self_coord.x) * chunk_size<float>,
            (float)(sc.y - self_coord.y) * chunk_size<float>,
        };
        const Range2D r_in_c{ r.min() + off, r.max() + off };
        const auto sw = sweep_aabb_vs_aabb(start, displacement, r_in_c);
        if (sw.has_collider)
        {
            res = sw;
            return path_search_continue::blocked;
        }
        return path_search_continue::pass;
    };

    Search::is_passable_(&c, c.world().neighbors(c.coord()), search_min, search_max, cb);
    return res;
}

namespace {

// on_blocker(step, collider, rect) returns the new bound: rects blocking only at or past it are
// skipped, and the walk ends once a cell's steps reach it.
template<typename F>
void sweep_line(world& w, chunk_coords_ ch, const rc::bresenham_line& line,
                uint32_t first_step, uint32_t last_step, Vector2ui bbox_size,
                const Search::pred& p, Grid::Pass::Pool* pool, F&& on_blocker)
{
    fm_assert(!pool || pool->params().bbox_size >= Math::max(bbox_size.x(), bbox_size.y()));
    const auto half = Vector2(bbox_size) * .5f;
    auto bound = (int64_t)last_step + 1;

    const auto meet = [](int64_t a0, int64_t a1, int64_t b0, int64_t b1, int64_t c0, int64_t c1) {
        const auto lo = Math::max(Math::max(a0, b0), c0), hi = Math::min(Math::min(a1, b1), c1);
        return lo <= hi ? lo : INT64_MAX;
    };

    // The first step >= k0 that a box overlapping r blocks, from the pixels of steps k0 to k1 and
    // the off-axis pixels of the moves from k0 - 1 to k1.
    const auto first_blocked = [&](Range2D r, uint32_t k0, uint32_t k1) -> int64_t {
        // centers whose box overlaps r, strictly as rect_intersects
        const auto x = line.steps_within(0, (int32_t)Math::floor(r.min().x() - half.x()) + 1,
                                            (int32_t)Math::ceil(r.max().x() + half.x()) - 1);
        const auto y = line.steps_within(1, (int32_t)Math::floor(r.min().y() - half.y()) + 1,
                                            (int32_t)Math::ceil(r.max().y() + half.y()) - 1);
        const int64_t x0 = x.first, x1 = x.last, y0 = y.first, y1 = y.last;
        const int64_t m0 = Math::max((int64_t)k0 - 1, (int64_t)first_step), m1 = Math::min((int64_t)k1, (int64_t)last_step - 1);
        const auto pixel = meet(x0, x1, y0, y1, k0, k1);
        // (x of t + 1, y of t) and (x of t, y of t + 1) block the move from t to t + 1
        const auto off0 = meet(x0 - 1, x1 - 1, y0, y1, m0, m1);
        const auto off1 = meet(x0, x1, y0 - 1, y1 - 1, m0, m1);
        return Math::min(pixel, Math::min(off0, off1) == INT64_MAX ? INT64_MAX : Math::min(off0, off1) + 1);
    };

    const auto visit = [&](const rc::detail::line_cell& cell) {
        const auto k0 = cell.first_step, k1 = cell.last_step;
        const auto a = line.pixel_at(k0 > first_step ? k0 - 1 : k0), b = line.pixel_at(k1 < last_step ? k1 + 1 : k1);
        const auto qmin = Vector2(Math::min(a, b)) - half, qmax = Vector2(Math::max(a, b)) + half;
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++)
            {
                auto* c = cell.chunks[(size_t)(j*3 + i)];
                if (!c)
                    continue;
                const auto shift = Vector2((cell.chunk_offset + Vector2i{i - 1, j - 1}) * chunk_size<int32_t>);
                const auto fmin = qmin - shift, fmax = qmax - shift;
                if (!rc::detail::within_chunk_bounds(fmin, fmax))
                    continue;
                c->rtree()->Search(fmin.data(), fmax.data(), [&](uint64_t data, const auto& r) {
                    const auto x = std::bit_cast<collision_data>(data);
                    if (x.pass == (uint64_t)pass_mode::pass)
                        return true;
                    const auto rect = Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}};
                    const auto step = first_blocked(Range2D{rect.min() + shift, rect.max() + shift}, k0, k1);
                    if (step >= bound)
                        return true;
                    // last, since without_critters looks up the object
                    if (p(*c, x, rect) == path_search_continue::pass)
                        return true;
                    bound = on_blocker(step, x, Range2D{rect.min() + shift, rect.max() + shift});
                    return true;
                });
            }
        // later cells hold steps after k1 only
        return bound > (int64_t)k1 + 1;
    };

    rc::detail::traverse_line_cells(w, ch, line, first_step, last_step, true, pool, visit);
}

} // namespace

swept_collider_result find_swept_collider(world& w, chunk_coords_ ch, const rc::bresenham_line& line,
                                          uint32_t first_step, uint32_t last_step, Vector2ui bbox_size,
                                          const Search::pred& p, Grid::Pass::Pool* pool)
{
    swept_collider_result res = { .step = last_step + 1, .collider = {}, .rect = {}, .has_collider = false };
    sweep_line(w, ch, line, first_step, last_step, bbox_size, p, pool, [&](int64_t step, collision_data x, Range2D rect) {
        res = { .step = (uint32_t)step, .collider = x, .rect = rect, .has_collider = true };
        return step;
    });
    return res;
}

void find_swept_collider(world& w, chunk_coords_ ch, const rc::bresenham_line& line,
                         uint32_t first_step, uint32_t last_step, Vector2ui bbox_size,
                         const Search::pred& p, Grid::Pass::Pool* pool,
                         fu2::function_view<bool(const swept_collider_result&) const> f)
{
    sweep_line(w, ch, line, first_step, last_step, bbox_size, p, pool, [&](int64_t step, collision_data x, Range2D rect) {
        const bool more = f(swept_collider_result{ .step = (uint32_t)step, .collider = x, .rect = rect, .has_collider = true });
        return more ? (int64_t)last_step + 1 : 0;
    });
}

} // namespace floormat
