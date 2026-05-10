#include "sweep-aabb.hpp"
#include "compat/limits.hpp"
#include "compat/function2.hpp"
#include "collision.hpp"
#include "chunk.hpp"
#include "world.hpp"
#include "search.hpp"
#include "rect-intersects.hpp"
#include "tile-constants.hpp"
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

    if (t_enter > t_exit || t_enter > 1.0f || t_exit <= 0.0f)
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

    constexpr auto chunk_extent = (float)tile_size_xy * (float)TILE_MAX_DIM;
    const auto self_coord = c.coord();

    sweep_result res = { .has_collider = false, /*.pos = limits<float>::max*/ };

    auto cb = [&](chunk& self, collision_data data, Range2D r) {
        if (data.type == (uint64_t)collision_type::none)
            return path_search_continue::pass;
        if (p(self, data, r) == path_search_continue::pass)
            return path_search_continue::pass;
        // search.cpp:107 shifts the search bbox into self's frame before firing
        // the pred, so r is in self-local coords; lift back into c's frame.
        const auto sc = self.coord();
        const Vector2 off{
            (float)(sc.x - self_coord.x) * chunk_extent,
            (float)(sc.y - self_coord.y) * chunk_extent,
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

} // namespace floormat
