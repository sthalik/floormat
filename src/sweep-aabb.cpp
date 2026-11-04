#include "sweep-aabb.hpp"
#include "compat/function2.hpp"
#include "collision.hpp"
#include "chunk.hpp"
#include "raycast-line.hpp"
#include "raycast-loop.hpp"
#include "grid-pass.hpp"
#include "RTree-search.hpp"
#include <bit>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

// on_blocker(step, collider, rect) returns the new bound: rects blocking only at or past it are
// skipped, and the walk ends once a cell's steps reach it.
template<typename F>
void sweep_aabb(world& w, chunk_coords_ ch, const rc::bresenham_line& line,
                u32 first_step, u32 last_step, Vector2ui bbox_size,
                const Search::pred& p, Grid::Pass::Pool* pool, F&& on_blocker)
{
    fm_assert(!pool || pool->params().bbox_size >= Math::max(bbox_size.x(), bbox_size.y()));
    // a box spans [center - lo, center + hi), as scenery_tile()
    const auto lo = Vector2i(bbox_size/2u), hi = Vector2i(bbox_size) - lo;
    auto bound = (i64)last_step + 1;

    const auto meet = [](i64 a0, i64 a1, i64 b0, i64 b1, i64 c0, i64 c1) {
        const auto lo = Math::max(Math::max(a0, b0), c0), hi = Math::min(Math::min(a1, b1), c1);
        return lo <= hi ? lo : INT64_MAX;
    };

    // The first step >= k0 that a box overlapping [r0, r1) blocks, from the pixels of steps k0 to
    // k1 and the orthogonal points of the moves from k0 - 1 to k1.
    const auto first_blocked = [&](Vector2i r0, Vector2i r1, u32 k0, u32 k1) -> i64 {
        // centers whose box overlaps [r0, r1), strictly as rect_intersects
        const auto x = line.interval_bounds_to_steps(0, r0.x() - hi.x() + 1, r1.x() + lo.x() - 1);
        const auto y = line.interval_bounds_to_steps(1, r0.y() - hi.y() + 1, r1.y() + lo.y() - 1);
        const i64 x0 = x.first, x1 = x.last, y0 = y.first, y1 = y.last;
        const i64 m0 = Math::max((i64)k0 - 1, (i64)first_step), m1 = Math::min((i64)k1, (i64)last_step - 1);
        const auto pixel = meet(x0, x1, y0, y1, k0, k1);
        // (x of t + 1, y of t) and (x of t, y of t + 1) block the move from t to t + 1
        const auto off0 = meet(x0 - 1, x1 - 1, y0, y1, m0, m1);
        const auto off1 = meet(x0, x1, y0 - 1, y1 - 1, m0, m1);
        return Math::min(pixel, Math::min(off0, off1) == INT64_MAX ? INT64_MAX : Math::min(off0, off1) + 1);
    };

    const auto visit = [&](const rc::detail::line_cell& cell) {
        const auto k0 = cell.first_step, k1 = cell.last_step;
        const auto a = line.point_at(k0 > first_step ? k0 - 1 : k0), b = line.point_at(k1 < last_step ? k1 + 1 : k1);
        const auto qmin = Math::min(a, b) - lo, qmax = Math::max(a, b) + hi;
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++)
            {
                auto* c = cell.chunks[(usize)(j*3 + i)];
                if (!c)
                    continue;
                const auto shift = (cell.chunk_offset + Vector2i{i - 1, j - 1}) * chunk_size<i32>;
                const auto imin = qmin - shift, imax = qmax - shift;
                if (!rc::detail::within_chunk_bounds(imin, imax))
                    continue;
                const auto fmin = Vector2(imin), fmax = Vector2(imax);
                c->rtree()->Search(fmin.data(), fmax.data(), [&](u64 data, const auto& r) {
                    const auto x = std::bit_cast<collision_data>(data);
                    if (x.pass == (u64)pass_mode::pass)
                        return true;
                    const auto rect = Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}};
                    const auto r0 = Vector2i(rect.min()) + shift, r1 = Vector2i(rect.max()) + shift;
                    fm_debug3_assert(Vector2(r0 - shift) == rect.min() && Vector2(r1 - shift) == rect.max());
                    const auto step = first_blocked(r0, r1, k0, k1);
                    if (step >= bound)
                        return true;
                    // last, since without_critters looks up the object
                    if (p(*c, x, rect) == path_search_continue::pass)
                        return true;
                    bound = on_blocker(step, x, Range2D{Vector2(r0), Vector2(r1)});
                    return true;
                });
            }
        // later cells hold steps after k1 only
        return bound > (i64)k1 + 1;
    };

    rc::detail::traverse_line_cells(w, ch, line, first_step, last_step, true, pool, visit);
}

} // namespace

swept_collider_result find_swept_collider(world& w, chunk_coords_ ch, const rc::bresenham_line& line,
                                          u32 first_step, u32 last_step, Vector2ui bbox_size,
                                          const Search::pred& p, Grid::Pass::Pool* pool)
{
    swept_collider_result res = { .step = last_step + 1, .collider = {}, .rect = {}, .has_collider = false };
    sweep_aabb(w, ch, line, first_step, last_step, bbox_size, p, pool, [&](i64 step, collision_data x, Range2D rect) {
        res = { .step = (u32)step, .collider = x, .rect = rect, .has_collider = true };
        return step;
    });
    return res;
}

void find_swept_collider(world& w, chunk_coords_ ch, const rc::bresenham_line& line,
                         u32 first_step, u32 last_step, Vector2ui bbox_size,
                         const Search::pred& p, Grid::Pass::Pool* pool,
                         fu2::function_view<bool(const swept_collider_result&) const> f)
{
    sweep_aabb(w, ch, line, first_step, last_step, bbox_size, p, pool, [&](i64 step, collision_data x, Range2D rect) {
        const bool more = f(swept_collider_result{ .step = (u32)step, .collider = x, .rect = rect, .has_collider = true });
        return more ? (i64)last_step + 1 : 0;
    });
}

} // namespace floormat
