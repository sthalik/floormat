#include "raycast-loop.hpp"
#include "raycast-line.hpp"
#include "raycast-diag.hpp"
#include "intra-coord.inl"
#include "point.inl"
#include "pass-mode.hpp"
#include "world.hpp"
#include "object.hpp"
#include "tile-bbox.hpp"
#include "grid-pass.hpp"
#include "grid-object.hpp"
#include "search-pred.hpp"
#include "RTree-search.hpp"
#include "compat/function-view.hpp"
#include <cfloat>
#include <bit>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat::Raycast::detail {

namespace {

using RTree = std::decay_t<decltype(*std::declval<class chunk>().rtree())>;
using Rect = RTree::Rect;

} // namespace

Vector2 dir_inverse(Vector2 dir)
{
#if !defined _MSC_VER || defined __clang__
    return Vector2{1} / dir;
#else
    // Under cl's -fp:fast, min/max don't keep the argument order that makes ray_aabb_intersection drop a NaN.
    // 1/1e-20 times a distance is still a finite float in the slab test.
    return Vector2{1} / Math::copysign(Math::max(Math::abs(dir), Vector2{1e-20f}), dir);
#endif
}

std::array<u8, 2> ray_aabb_signs(Vector2 ray_dir_inv_norm)
{
    bool signs[2];
    for (unsigned d = 0; d < 2; ++d)
        signs[d] = std::signbit(ray_dir_inv_norm[d]);
    return { signs[0], signs[1] };
}

// https://tavianator.com/2022/ray_box_boundary.html
// https://www.researchgate.net/figure/The-slab-method-for-ray-intersection-detection-15_fig3_283515372
aabb_result ray_aabb_intersection(Vector2 ray_origin, Vector2 ray_dir_inv_norm,
                                  std::array<Vector2, 2> box_minmax, std::array<u8, 2> signs)
{
    using Math::min;
    using Math::max;

    float tmin = 0, tmax = 16777216;

    for (unsigned d = 0; d < 2; ++d)
    {
        auto bmin = box_minmax[signs[d]][d];
        auto bmax = box_minmax[!signs[d]][d];
        float dmin = (bmin - ray_origin[d]) * ray_dir_inv_norm[d];
        float dmax = (bmax - ray_origin[d]) * ray_dir_inv_norm[d];

        // Magnum's min/max drop a NaN in the second argument. A ray along a box
        // side gives 0 * inf = NaN here, so this order counts the side as a hit.
        tmin = max(tmin, dmin);
        tmax = min(tmax, dmax);
    }

    return { tmin, tmin < tmax };
}

aabb_result ray_rounded_rect_intersection(Vector2 ray_origin, Vector2 ray_dir, Vector2 ray_dir_inv_norm,
                                          std::array<Vector2, 2> box_minmax, float radius,
                                          std::array<u8, 2> signs)
{
    const auto [bmin, bmax] = box_minmax;
    const auto grown = ray_aabb_intersection(ray_origin, ray_dir_inv_norm,
                                             {{bmin - Vector2{radius}, bmax + Vector2{radius}}}, signs);
    // radius 0 skips the corner test, since rounding can put the entry point outside the box on both axes.
    if (!grown.result || radius == 0)
        return { grown.tmin, grown.result };

    // An entry outside the box on both axes lies in a corner square of the enlarged box. Crossing from there
    // into an edge strip passes through the corner's circle, so the circle test gives the result.
    const auto p = ray_origin + ray_dir * grown.tmin;
    const bool out_x = p.x() < bmin.x() || p.x() > bmax.x(),
               out_y = p.y() < bmin.y() || p.y() > bmax.y();
    if (!out_x || !out_y)
        return { grown.tmin, true };

    // Ericson, Real-Time Collision Detection, 5.3.2
    const Vector2 corner{ p.x() < bmin.x() ? bmin.x() : bmax.x(),
                          p.y() < bmin.y() ? bmin.y() : bmax.y() };
    const auto m = ray_origin - corner;
    const float b = Math::dot(m, ray_dir);
    const float c = Math::dot(m, m) - radius*radius;
    if (c > 0 && b > 0)
        return { 0, false };
    // b*b - c loses most digits far from the corner, the squared distance to the line doesn't. Haines,
    // Günther, Akenine-Möller, "Precision Improvements for Ray/Sphere Intersection", Ray Tracing Gems, 2019.
    const auto perp = m - ray_dir * b;
    const float discr = radius*radius - Math::dot(perp, perp);
    if (discr < 0)
        return { 0, false };
    return { Math::max(0.f, -b - Math::sqrt(discr)), true };
}

template<typename Pool>
void traverse_line_cells(world& w, chunk_coords_ ch, const bresenham_line& line,
                         u32 first_step, u32 last_step, bool both_orthogonal,
                         Pool pool, const Search::pred& p, std::array<Array<bool>, 4>* entry_blocked,
                         function_view<bool(const line_cell&) const> visit,
                         object_cell_visitor<u16> visit_objects_u16, object_cell_visitor<u32> visit_objects_u32)
{
    static_assert(std::is_null_pointer_v<Pool> || std::is_same_v<Pool, Grid::Pass::Pool*> ||
                  std::is_same_v<Pool, Grid::Object::Pool*>);
    constexpr bool has_pool = !std::is_null_pointer_v<Pool>, has_object_pool = std::is_same_v<Pool, Grid::Object::Pool*>;
    fm_assert(first_step <= last_step && last_step <= line.steps());
    if constexpr (has_object_pool)
        fm_assert(pool && entry_blocked);
    else if constexpr (has_pool)
        fm_assert(pool);

    constexpr auto csize = chunk_size<i32>;
    const auto div = [&] {
        if constexpr (std::is_null_pointer_v<Pool>)
            return csize;
        else
            return (i32)pool->params().div_size();
    }();
    const auto div_count = (u32)(csize / div);
    const auto div_shift = std::countr_zero((u32)div); // xxx todo assumes power of two chunk size
    const auto origin = [&] {
        if constexpr (std::is_null_pointer_v<Pool>)
            return Vector2i{half_tile<i32>};
        else
            return Vector2i{half_tile<i32> - (i32)pool->params().node_offset()};
    }();
    const auto cell_of = [&](Vector2i p) {
        return Vector2i{(p.x() + origin.x()) >> div_shift, (p.y() + origin.y()) >> div_shift};
    };

    const auto major = line.major_axis(), minor = 1 - major;
    const auto from = line.start_pos();
    const u64 len = line.steps(), minor_len = line.minor_len();
    // The steps with minor offset at most b end before t = ceil((2b + 1)*len / (2*minor_len)). Keeping
    // t_rem = t*2*minor_len - (2b + 1)*len lets the next cell's b + div skip the division.
    i64 t_b = INT32_MIN;
    u64 t = 0, t_rem = 0, t_step = 0, t_step_rem = 0;
    if (minor_len)
    {
        t_step = 2*(u64)div*len / (2*minor_len);
        t_step_rem = 2*(u64)div*len % (2*minor_len);
    }
    const auto last_within_minor = [&](i64 b) -> u32 {
        if (b >= (i64)minor_len)
            return (u32)len;
        const auto d = 2*minor_len;
        if (b == t_b + div)
        {
            t += t_step;
            if (t_rem < t_step_rem)
            {
                t_rem += d - t_step_rem;
                t++;
            }
            else
                t_rem -= t_step_rem;
        }
        else if (b != t_b)
        {
            const auto a = (2*(u64)b + 1)*len;
            t = (a + d - 1)/d;
            t_rem = t*d - a;
        }
        t_b = b;
        return (u32)(t - 1);
    };

    line_cell cell = {};
    Vector2i loaded{INT32_MAX, INT32_MAX};
    Pass::Grid grid{nullptr, nullptr};
    Grid::Object::Grid object_grid{nullptr};
    bool has_grid = false, has_objects = false, chunk_empty = false;
    const usize* corner_bits[4] = {};
    std::array<Grid::Object::Grid, 4> corner_objects{Grid::Object::Grid{nullptr}, Grid::Object::Grid{nullptr},
                                                     Grid::Object::Grid{nullptr}, Grid::Object::Grid{nullptr}};
    bool corner_has_objects[4] = {};
    u32 corners_loaded = 0;

    const auto chunks_3x3 = [&](Vector2i off) -> std::array<chunk*, 9> {
        std::array<chunk*, 9> chunks;
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++)
                chunks[(usize)(j*3 + i)] = w.at(chunk_coords_{
                    (i16)(ch.x + off.x() + i - 1), (i16)(ch.y + off.y() + j - 1), ch.z });
        return chunks;
    };
    // Callers compare with loaded first, so a cell in the loaded chunk makes no call.
    const auto load = [&](Vector2i off) {
        loaded = off;
        cell.chunk_offset = off;
        cell.chunks = chunks_3x3(off);
        auto* c = cell.chunks[4];
        if constexpr (!std::is_null_pointer_v<Pool>)
            has_grid = c != nullptr;
        has_objects = false;
        if (has_grid)
        {
            if constexpr (std::is_same_v<Pool, Grid::Object::Pool*>)
            {
                grid = pool->pass_pool()[*c];
                grid.build_if_stale();
                object_grid = (*pool)[*c];
                object_grid.build_if_stale();
                has_objects = object_grid.blocked_objects(p, (*entry_blocked)[0]);
            }
            else if constexpr (std::is_same_v<Pool, Grid::Pass::Pool*>)
            {
                grid = (*pool)[*c];
                grid.build_if_stale();
            }
            chunk_empty = grid.is_all_empty() && !has_objects;
        }
        else if (!c)
        {
            // colliders overhang from loaded neighbors into a missing chunk
            chunk_empty = true;
            for (auto* nb : cell.chunks)
                chunk_empty &= !nb;
        }
        else
            chunk_empty = false;
        if constexpr (has_pool)
        {
            corner_bits[0] = has_grid ? grid.bits().data : nullptr;
            corner_objects[0] = object_grid;
            corner_has_objects[0] = has_objects;
            corners_loaded = 1;
        }
    };
    const auto load_corner_chunk = [&](u32 n) {
        corners_loaded |= 1u << n;
        corner_bits[n] = nullptr;
        corner_has_objects[n] = false;
        auto* c = cell.chunks[4 + (n & 1) + 3*(n >> 1)];
        if (!c)
            return;
        if constexpr (std::is_same_v<Pool, Grid::Object::Pool*>)
        {
            auto g = pool->pass_pool()[*c];
            g.build_if_stale();
            corner_bits[n] = g.bits().data;
            auto og = (*pool)[*c];
            og.build_if_stale();
            corner_has_objects[n] = og.blocked_objects(p, (*entry_blocked)[n]);
            corner_objects[n] = og;
        }
        else if constexpr (std::is_same_v<Pool, Grid::Pass::Pool*>)
        {
            auto g = (*pool)[*c];
            g.build_if_stale();
            corner_bits[n] = g.bits().data;
        }
    };
    const auto test_bit = [](const usize* b, u32 i) -> bool {
        return b[i / (sizeof(usize)*8)] >> i % (sizeof(usize)*8) & 1;
    };
    const auto test_bit_pair = [](const usize* b, u32 i) -> bool {
        constexpr u32 W = sizeof(usize)*8;
        if (i % W != W - 1) [[likely]]
            return (~b[i / W] >> i % W & 3) == 0;
        else
            return b[i / W] >> (W - 1) & b[i / W + 1] & 1;
    };
    const auto corner_is_set = [&](u32 x, u32 y) -> bool {
        const u32 n = (x == div_count) + 2*(y == div_count);
        if (!(corners_loaded & 1u << n))
            load_corner_chunk(n);
        const auto* b = corner_bits[n];
        return b && test_bit(b, (y & (div_count - 1))*div_count + (x & (div_count - 1)));
    };
    const auto square_is_set = [&](u32 x, u32 y, bool x1, bool y1) {
        return corner_is_set(x, y) && (!x1 || corner_is_set(x + 1, y)) && (!y1 || corner_is_set(x, y + 1))
               && (!x1 || !y1 || corner_is_set(x + 1, y + 1));
    };

    // v[axis] with a run-time axis goes through the stack.
    const auto on = [](Vector2i v, u32 axis) { return axis ? v.y() : v.x(); };
    const auto axis_step = [&](u32 axis) {
        const auto s = line.sign(axis);
        return axis ? Vector2i{0, s} : Vector2i{s, 0};
    };
    const auto major_step = axis_step(major), minor_step = axis_step(minor);
    const auto count_shift = std::countr_zero(div_count); // xxx todo assumes power of two chunk size
    const auto count_mask = (i32)div_count - 1;
    const auto chunk_of = [&](Vector2i c) { return Vector2i{c.x() >> count_shift, c.y() >> count_shift}; };

    [[maybe_unused]] bool corners_x1 = true, corners_y1 = true;
    if constexpr (has_pool)
        if (!minor_len && ((on(from, minor) + on(origin, minor)) & (div - 1)) == 0)
            (minor ? corners_y1 : corners_x1) = false;
    const auto visit_corner_objects = [&]<typename T>(u32 x, u32 y) -> bool {
        const u32 n = (x == div_count) + 2*(y == div_count);
        if (!corner_has_objects[n])
            return true;
        const auto& g = corner_objects[n];
        const ArrayView<const bool> blocked = (*entry_blocked)[n];
        const auto i = (y & (div_count - 1))*div_count + (x & (div_count - 1));
        const auto visit_at = [&]<typename U>(const object_cell_visitor<U>& f) {
            if (!g.is_blocked<U>(i, blocked))
                return true;
            else if (!n)
                return f(cell, g, g.objects_at_cell<U>(i), blocked);
            else
            {
                const auto off = loaded + Vector2i{(i32)(n & 1), (i32)(n >> 1)};
                return f(line_cell{chunks_3x3(off), off, cell.first_step, cell.last_step}, g, g.objects_at_cell<U>(i), blocked);
            }
        };
        if (n ? g.uses_u32() : std::is_same_v<T, u32>)
            return visit_at(visit_objects_u32);
        else
            return visit_at(visit_objects_u16);
    };
    const auto visit_square_objects = [&]<typename T>(u32 x, u32 y, bool x1, bool y1) -> bool {
        return visit_corner_objects.template operator()<T>(x, y)
               && (!x1 || visit_corner_objects.template operator()<T>(x + 1, y))
               && (!y1 || visit_corner_objects.template operator()<T>(x, y + 1))
               && (!x1 || !y1 || visit_corner_objects.template operator()<T>(x + 1, y + 1));
    };

    // Cells count from the line's chunk, so a cell's chunk and its cell within it are computed with a shift and
    // a mask. global_cell moves to the cell the line exits into, or to that of point_at(k) when region is true.
    Vector2i global_cell = cell_of(line.point_at(first_step));
    auto k = first_step;
    bool run_pending = false;
    u32 run_first = 0, run_last = 0;
    const auto flush_run = [&] {
        if (!run_pending)
            return true;
        run_pending = false;
        return visit(line_cell{cell.chunks, cell.chunk_offset, run_first, run_last});
    };
    // Returns false after loading a chunk whose object grid isn't T,
    // so the loop over cells continues with the other index type.
    const auto walk = [&]<typename T>() -> bool {
        constexpr bool is_u32 = std::is_same_v<T, u32>;
        for (;;)
        {
            const auto chunk_off = chunk_of(global_cell);
            const auto local_cell = Vector2i{global_cell.x() & count_mask, global_cell.y() & count_mask};
            if (chunk_off != loaded)
            {
                if constexpr (has_pool)
                    if (!flush_run())
                        return true;
                load(chunk_off);
                if (has_objects && object_grid.uses_u32() != is_u32)
                    return false;
            }
            Vector2i lo, hi;
            const auto x = (u32)local_cell.x(), y = (u32)local_cell.y();
            const bool region = chunk_empty && (!has_pool || x + 1 < div_count && y + 1 < div_count);
            bool skip = region;
            if (region)
            {
                lo = chunk_off * csize - origin;
                hi = lo + Vector2i{has_pool ? csize - div - 1 : csize - 1};
            }
            else
            {
                lo = chunk_off * csize + local_cell * div - origin;
                hi = lo + Vector2i{div - 1};
                if constexpr (has_pool)
                {
                    if (x + 1 < div_count && y + 1 < div_count) [[likely]]
                    {
                        const auto* b = corner_bits[0];
                        const auto i = y*div_count + x, i2 = i + div_count;
                        skip = b && (corners_x1 ? test_bit_pair(b, i) : test_bit(b, i))
                                 && (!corners_y1 || (corners_x1 ? test_bit_pair(b, i2) : test_bit(b, i2)));
                    }
                    else
                        skip = square_is_set(x, y, corners_x1, corners_y1);
                }
            }
            fm_debug3_assert((line.point_at(k) >= lo).all() && (line.point_at(k) <= hi).all());
            const auto b = line.sign(minor) > 0 ? (i64)on(hi, minor) - on(from, minor) : (i64)on(from, minor) - on(lo, minor);
            const auto last_major = line.interval_bounds_to_steps(major, on(lo, major), on(hi, major)).last, last_minor = last_within_minor(b);
            const auto last = Math::min(Math::min(last_major, last_minor), last_step);
            cell.first_step = k;
            cell.last_step = last;
            if (!skip)
            {
                if constexpr (has_pool)
                {
                    if (!run_pending)
                        run_first = k;
                    run_pending = true;
                    run_last = last;
                }
                else if (!visit(cell))
                    return true;
            }
            else if constexpr (has_pool)
            {
                if (!flush_run())
                    return true;
                if constexpr (has_object_pool)
                {
                    const auto i = y*div_count + x, n = corners_x1 ? 2u : 1u;
                    const bool empty = x + 1 < div_count && y + 1 < div_count
                                       && (!has_objects || object_grid.is_empty_at<T>(i, n)
                                           && (!corners_y1 || object_grid.is_empty_at<T>(i + div_count, n)));
                    if (!empty && !visit_square_objects.template operator()<T>(x, y, corners_x1, corners_y1))
                        return true;
                }
            }
            if (last == last_step)
            {
                if constexpr (has_pool)
                    flush_run();
                return true;
            }

            if (!region)
            {
                const bool exit_major = last == last_major, exit_minor = last == last_minor;
                const auto next = global_cell + (exit_major ? major_step : Vector2i{}) + (exit_minor ? minor_step : Vector2i{});
                // Only a step out through both sides at once has orthogonal points outside this cell and the next.
                // Point i moves along axis i first, and the minor offset at step last is b, as in orthogonal_at().
                if (exit_major && exit_minor)
                {
                    const auto e = (i64)((2*(u64)last + 1)*minor_len) - (i64)((2*(u64)b + 1)*len);
                    for (auto i = 0u; i < 2; i++)
                    {
                        if (!both_orthogonal && !(i == major ? e <= 0 : e >= 0))
                            continue;
                        const auto corner = i == 0 ? Vector2i{next.x(), global_cell.y()} : Vector2i{global_cell.x(), next.y()};
                        const auto corner_off = chunk_of(corner);
                        if constexpr (has_pool)
                        {
                            const auto cx = (u32)(corner.x() & count_mask), cy = (u32)(corner.y() & count_mask);
                            const bool x1 = (line.sign(0) > 0) != (i == 0), y1 = (line.sign(1) > 0) != (i == 1);
                            if (corner_off == loaded && square_is_set(cx, cy, x1, y1))
                            {
                                if constexpr (has_object_pool)
                                    if (!visit_square_objects.template operator()<T>(cx, cy, x1, y1))
                                    {
                                        flush_run();
                                        return true;
                                    }
                                continue;
                            }
                        }
                        const auto chunks = corner_off == loaded ? cell.chunks : chunks_3x3(corner_off);
                        if (!visit(line_cell{chunks, corner_off, last + 1, last + 1}))
                        {
                            if constexpr (has_pool)
                                flush_run();
                            return true;
                        }
                    }
                }
                global_cell = next;
                k = last + 1;
                continue;
            }

            // A diagonal step through the corner of four cells puts its orthogonal points in two cells neither
            // endpoint of the step is in. They are visited without a bit lookup, since such steps are rare.
            if (const auto o = line.orthogonal_at(last); o.diagonal)
            {
                const auto c0 = cell_of(line.point_at(last)), c1 = cell_of(line.point_at(last + 1));
                for (auto i = 0u; i < 2; i++)
                {
                    if (!both_orthogonal && !o.in_supercover[i])
                        continue;
                    const auto c = cell_of(o.points[i]);
                    if (c == c0 || c == c1)
                        continue;
                    const auto c_off = chunk_of(c);
                    const auto chunks = c_off == loaded ? cell.chunks : chunks_3x3(c_off);
                    if (!visit(line_cell{chunks, c_off, last + 1, last + 1}))
                        return true;
                }
            }
            k = last + 1;
            global_cell = cell_of(line.point_at(k));
        }
    };
    if constexpr (std::is_same_v<Pool, Grid::Object::Pool*>)
    {
        for (bool done = false; !done; )
            done = walk.template operator()<u16>() || walk.template operator()<u32>();
    }
    else
        walk.template operator()<u16>();
}

namespace {

struct line_pixel
{
    // 2k for the pixel of step k, 2k + 1 for an orthogonal point of the step from k to k + 1
    i64 key;
    Vector2i pixel;
};

// Returns the first pixel of the line's supercover in the closed rect [lo, hi], or bound when no pixel in the rect
// has a key below bound.
line_pixel first_pixel_in_bounds(const bresenham_line& line, Vector2i lo, Vector2i hi, i64 bound)
{
    const auto x = line.interval_bounds_to_steps(0, lo.x(), hi.x()), y = line.interval_bounds_to_steps(1, lo.y(), hi.y());
    const i64 x0 = x.first, x1 = x.last, y0 = y.first, y1 = y.last;
    line_pixel ret = { bound, {} };
    if (const auto k = Math::max(x0, y0); k <= Math::min(x1, y1) && 2*k < bound)
        ret = { 2*k, line.point_at((u32)k) };
    // The steps putting orthogonal point i in the rect form a range. If its first step's point i
    // isn't in the supercover, a step's own pixel, point_at(k), in the rect comes before every later one.
    for (auto i = 0u; i < 2; i++)
    {
        const auto t  = i == 0 ? Math::max(x0 - 1, y0) : Math::max(x0, y0 - 1);
        const auto t1 = i == 0 ? Math::min(x1 - 1, y1) : Math::min(x1, y1 - 1);
        if (t > Math::min(t1, (i64)line.steps() - 1) || 2*t + 1 >= ret.key)
            continue;
        if (const auto o = line.orthogonal_at((u32)t); o.diagonal && o.in_supercover[i])
            ret = { 2*t + 1, o.points[i] };
    }
    return ret;
}

} // namespace

template<bool EnableDiagnostics, bool HasObjectPool>
raycast_result_s do_raycasting(std::conditional_t<EnableDiagnostics, raycast_diag_s&, std::nullptr_t> diag,
                               world& w, point from, point to, object_id self, Grid::Pass::Pool& pass_pool,
                               std::conditional_t<HasObjectPool, Grid::Object::Pool&, std::nullptr_t> objects,
                               const Search::pred& pred, float radius)
{
    raycast_result_s result;
    fm_assert(from.chunk3().z == to.chunk3().z);

    using Math::abs;
    using Math::floor;

    constexpr auto inv_eps = 1e6f, eps = 1/inv_eps;
    constexpr auto fuzz2 = 0.5f;

    auto V = Vector2(to - from);
    auto ray_len = V.length();

    if (ray_len < eps)
    {
        if constexpr(EnableDiagnostics)
        {
            diag.V = V;
            diag.dir = {};
            diag.dir_inv_norm = {};
            diag.tmin = 0;
            diag.radius = radius;
            arrayClear(diag.path);
            arrayClear(diag.queries);
        }
        result = {
            .from = from,
            .to = to,
            .collision = {},
            .collider = {
                .type = (u64)collision_type::none,
                .pass = (u64)pass_mode::pass,
                .id   = ((u64)1 << collision_data_BITS)-1,
            },
            .has_result = true,
            .success = true,
        };
        return result;
    }
    auto dir = V * (1.f/ray_len);
    auto dir_inv_norm = dir_inverse(dir);
    auto signs = ray_aabb_signs(dir_inv_norm);

    result = {
        .from = from,
        .to = to,
        .collision = {},
        .collider = {
            .type = (u64)collision_type::none,
            .pass = (u64)pass_mode::pass,
            .id   = ((u64)1 << collision_data_BITS)-1,
        },
        .has_result = true,
        .success = false,
    };

    const auto params = pass_pool.params();
    fm_assert(radius >= 0);
    fm_assert(min_pool_bbox_size(radius) <= params.bbox_size);
    fm_assert(params.min_pass != pass_mode::pass || params.div_size() == 1);
    // R-tree rects have integer corners and Search() counts a touching rect, so a cell grown by
    // floor(fuzz2 + radius) finds the same rects as one grown by fuzz2 + radius.
    const auto grow = (i32)(fuzz2 + radius);
    const auto div_size_i = (i32)params.div_size();
    const auto min_pass = (u64)params.min_pass;
    const auto div_size_f = (float)div_size_i;
    const auto cells_per_chunk = (i32)params.div_count;

    const auto from_ic = intra_coord{from};
    const auto from_shifted = Vector2(from_ic);
    const auto from_center = Vector2(from_ic.center_shifted());
    const auto from_px = from - point{from.chunk3(), {}, {}};
    const auto line = bresenham_line{from_px, from_px + (to - from)};
    const auto step_len = ray_len / (float)line.steps();

    i32 cell_x = from_ic.x() / div_size_i, cell_y = from_ic.y() / div_size_i;
    i32 chunk_off_x = 0, local_cell_x = cell_x;
    i32 chunk_off_y = 0, local_cell_y = cell_y;

    int step_x = dir.x() > 0 ? 1 : (dir.x() < 0 ? -1 : 0);
    int step_y = dir.y() > 0 ? 1 : (dir.y() < 0 ? -1 : 0);

    float t_max_x = step_x == 0 ? FLT_MAX :
        (((step_x > 0 ? (float)(cell_x + 1) : (float)cell_x) * div_size_f) - from_shifted.x()) / dir.x();
    float t_max_y = step_y == 0 ? FLT_MAX :
        (((step_y > 0 ? (float)(cell_y + 1) : (float)cell_y) * div_size_f) - from_shifted.y()) / dir.y();
    float t_delta_x = step_x == 0 ? FLT_MAX : abs(div_size_f / dir.x());
    float t_delta_y = step_y == 0 ? FLT_MAX : abs(div_size_f / dir.y());

    if constexpr(EnableDiagnostics)
    {
        auto path = move(diag.path);
        auto queries = move(diag.queries);
        diag = {
            .path = move(path),
            .queries = move(queries),
            .V = V,
            .dir = dir,
            .dir_inv_norm = dir_inv_norm,
            .tmin = 0,
            .radius = radius,
        };
        auto chunk_count_max = (u32)(abs(V.x()) / chunk_size<float>
                                        + abs(V.y()) / chunk_size<float>) + 2u;
        arrayClear(diag.path);
        arrayClear(diag.queries);
        arrayReserve(diag.path, chunk_count_max);
        arrayReserve(diag.queries, 8u);
    }

    Vector2i last_chunk_off { (i32)1<<30, (i32)1<<30 };
    chunk* last_nb[9] = {};
    chunk* last_c = nullptr;
    Pass::Grid last_g{nullptr, nullptr};
    Grid::Object::Grid last_og{nullptr};
    bool last_pass_grid_built = false, last_has_objects = false;
    bool last_chunk_empty = true;

    float min_tmin = FLT_MAX;
    i64 best_key = INT64_MAX;
    Vector2i best_px;
    bool b = true;
    float t_entry = 0;

    const auto find_intersecting_collider = [&]<bool UsingRayPredicate>(std::bool_constant<UsingRayPredicate>,
                                                                        chunk& nb, Vector2i nb_world,
                                                                        collision_data x, const Range2D& range) {
        if (radius == 0)
        {
            // a rect covers the pixels in [min, max), as for critter movement
            const auto hit = first_pixel_in_bounds(line, Vector2i(Math::ceil(range.min())) + nb_world,
                                                   Vector2i(Math::ceil(range.max())) - Vector2i{1} + nb_world,
                                                   best_key);
            if (hit.key >= best_key) [[likely]]
                return;
            // The predicate runs last, since without_critters looks up the object.
            if constexpr (UsingRayPredicate)
                if (pred(nb, x) == path_search_continue::pass)
                    return;
            best_key = hit.key;
            best_px = hit.pixel;
            // The ray crosses the square of a pixel with a lower key by step key/2, in the cell whose R-tree query
            // returns its rect. One more step covers t_entry's rounding.
            min_tmin = (float)(hit.key/2 + 1) * step_len;
            result.collision = point::normalize_coords(from, hit.pixel - from_px);
        }
        else
        {
            const auto origin = from_center - Vector2(nb_world);
            const auto ret = ray_rounded_rect_intersection(origin, dir, dir_inv_norm,
                                                           {{range.min() - Vector2{fuzz2},
                                                             range.max() - Vector2{fuzz2}}},
                                                           radius, signs);
            if (!ret.result || ret.tmin > ray_len || ret.tmin >= min_tmin)
                return;
            if constexpr (UsingRayPredicate)
                if (pred(nb, x) == path_search_continue::pass)
                    return;
            min_tmin = ret.tmin;
            const auto contact = Math::clamp(origin + dir * min_tmin, range.min(), range.max() - Vector2{1});
            result.collision = point::normalize_coords(from, Vector2i(contact - origin));
        }
        result.collider = x;
        b = false;
    };

    // Returns false after building a chunk's object grid that isn't T,
    // so the loop over cells continues with the other index type.
    const auto walk = [&]<typename T>() -> bool {
        // A hit's tmin can lie up to a full rect extent past the cell whose query returned it, so the loop goes on
        // until the cell entry distance passes the best hit.
        while (t_entry <= ray_len + eps && (b || t_entry <= min_tmin))
        {
            Vector2i this_chunk_off{chunk_off_x, chunk_off_y};
            if (this_chunk_off != last_chunk_off)
            {
                last_chunk_off = this_chunk_off;
                for (int j = 0; j < 3; j++)
                    for (int i = 0; i < 3; i++)
                    {
                        chunk_coords_ co {
                            (i16)(from.chunk().x + chunk_off_x + i - 1),
                            (i16)(from.chunk().y + chunk_off_y + j - 1),
                            from.chunk3().z };
                        last_nb[j*3 + i] = w.at(co);
                    }
                last_c = last_nb[4];
                last_pass_grid_built = false;
                if (!last_c)
                {
                    // colliders overhang from loaded neighbors into a missing chunk.
                    // skip it only when the whole 3x3 area is unloaded.
                    last_chunk_empty = true;
                    for (auto* nb : last_nb)
                        last_chunk_empty &= !nb;
                }

                if constexpr(EnableDiagnostics)
                {
                    chunk_coords_ ch_coord {
                        (i16)(from.chunk().x + chunk_off_x),
                        (i16)(from.chunk().y + chunk_off_y),
                        from.chunk3().z };
                    arrayAppend(diag.path, bbox{
                        point{ch_coord, intra_coord{chunk_size<Vector2i> / 2}},
                        chunk_size<Vector2ui>,
                    });
                }
            }

            bool bit = true;
            if (last_c)
            {
                if (!last_pass_grid_built)
                {
                    last_g = pass_pool[*last_c];
                    last_g.build_if_stale();
                    last_pass_grid_built = true;
                    if constexpr (HasObjectPool)
                    {
                        last_og = objects[*last_c];
                        last_og.build_if_stale();
                        last_has_objects = last_og.blocked_objects(pred);
                    }
                    last_chunk_empty = last_g.is_all_empty() && !last_has_objects;
                    if constexpr (HasObjectPool)
                        if (last_has_objects && last_og.uses_u32() != std::is_same_v<T, u32>)
                            return false;
                }
                if (!last_chunk_empty)
                {
                    auto bit_idx = Pass::Grid::get_bitmask_index(
                        (u32)local_cell_x, (u32)local_cell_y, (u32)cells_per_chunk);
                    bit = last_g.bit(bit_idx);
                    if constexpr (HasObjectPool)
                    {
                        if (bit && last_has_objects)
                        {
                            const auto objects = last_og.objects_at_cell<T>(bit_idx);
                            const auto chunks = last_og.entry_chunks();
                            for (auto i = 0u; i < objects.size(); i++)
                            {
                                const auto& x = objects[i];
                                if (x.id == self)
                                    continue;
                                const auto e = objects.entry_index(i);
                                const auto off = chunks[e] ? Vector2i(world::neighbor_offsets[chunks[e] - 1]) : Vector2i{};
                                auto* nb = last_nb[(off.y() + 1)*3 + off.x() + 1];
                                fm_debug3_assert(nb);
                                if (pred(*nb, x) == path_search_continue::pass)
                                    continue;
                                const auto* o = std::as_const(w).find_object(x.id);
                                fm_debug3_assert(o);
                                const auto box = scenery_tile(o->coord.local(), o->offset, o->bbox_offset, o->bbox_size);
                                find_intersecting_collider(std::false_type{}, *nb,
                                                           (this_chunk_off + off) * chunk_size<i32>, x,
                                                           Range2D{Vector2(box.first()), Vector2(box.second())});
                            }
                        }
                    }
                }
            }
            else
                bit = last_chunk_empty;

            if (!bit)
            {
                const auto query_size = Vector2i{div_size_i + 2*grow};
                const auto query_min = Vector2i{cell_x, cell_y} * div_size_i - Vector2i{half_tile<i32> + grow};
                const auto query_max = query_min + query_size;
                if constexpr(EnableDiagnostics)
                    arrayAppend(diag.queries, bbox{point{from.chunk3(), {}, {}} + (query_min + query_size/2), Vector2ui(query_size)});

                for (int i = 0; i < 3; i++)
                {
                    for (int j = 0; j < 3; j++)
                    {
                        auto* nb = last_nb[j*3 + i];
                        if (!nb)
                            continue;
                        i32 nb_off_x = chunk_off_x + i - 1;
                        i32 nb_off_y = chunk_off_y + j - 1;

                        const auto nb_world = Vector2i{nb_off_x, nb_off_y} * chunk_size<i32>;
                        const auto fmin = Vector2(query_min - nb_world), fmax = Vector2(query_max - nb_world);

                        if (!within_chunk_bounds(fmin, fmax))
                            continue;

                        nb->rtree()->Search(fmin.data(), fmax.data(), [&, min_pass](u64 data, const Rect& r)
                        {
                            auto x = std::bit_cast<collision_data>(data);
                            fm_debug3_assert(x.type != (u64)collision_type::none);
                            if (x.id == self || x.pass >= min_pass)
                                return true;
                            find_intersecting_collider(std::true_type{}, *nb, nb_world, x,
                                                       Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}});
                            return true;
                        });
                    }
                }
            }

            if (last_chunk_empty)
            {
                i32 nx = (step_x > 0) ? (cells_per_chunk - local_cell_x)
                           : (step_x < 0) ? (local_cell_x + 1)
                           : INT32_MAX;
                i32 ny = (step_y > 0) ? (cells_per_chunk - local_cell_y)
                           : (step_y < 0) ? (local_cell_y + 1)
                           : INT32_MAX;

                float t_x_chunk = (step_x == 0) ? FLT_MAX : t_max_x + (float)(nx - 1) * t_delta_x;
                float t_y_chunk = (step_y == 0) ? FLT_MAX : t_max_y + (float)(ny - 1) * t_delta_y;

                if (t_x_chunk <= t_y_chunk)
                {
                    i32 y_crossings = 0;
                    if (step_y != 0 && t_max_y <= t_x_chunk)
                        y_crossings = (i32)floor((t_x_chunk - t_max_y) / t_delta_y) + 1;
                    if (y_crossings >= ny) y_crossings = ny - 1;
                    if (y_crossings < 0) y_crossings = 0;

                    cell_x += step_x * nx;
                    local_cell_x = (step_x > 0) ? 0 : cells_per_chunk - 1;
                    chunk_off_x += step_x;
                    t_entry = t_x_chunk;
                    t_max_x = t_x_chunk + t_delta_x;

                    cell_y += step_y * y_crossings;
                    local_cell_y += step_y * y_crossings;
                    if (step_y != 0)
                        t_max_y += (float)y_crossings * t_delta_y;
                }
                else
                {
                    i32 x_crossings = 0;
                    if (step_x != 0 && t_max_x <= t_y_chunk)
                        x_crossings = (i32)floor((t_y_chunk - t_max_x) / t_delta_x) + 1;
                    if (x_crossings >= nx) x_crossings = nx - 1;
                    if (x_crossings < 0) x_crossings = 0;

                    cell_y += step_y * ny;
                    local_cell_y = (step_y > 0) ? 0 : cells_per_chunk - 1;
                    chunk_off_y += step_y;
                    t_entry = t_y_chunk;
                    t_max_y = t_y_chunk + t_delta_y;

                    cell_x += step_x * x_crossings;
                    local_cell_x += step_x * x_crossings;
                    if (step_x != 0)
                        t_max_x += (float)x_crossings * t_delta_x;
                }
            }
            else if (t_max_x < t_max_y)
            {
                t_entry = t_max_x;
                t_max_x += t_delta_x;
                cell_x += step_x;
                local_cell_x += step_x;
                if (local_cell_x == cells_per_chunk) { local_cell_x = 0; chunk_off_x += 1; }
                else if (local_cell_x == -1) { local_cell_x = cells_per_chunk - 1; chunk_off_x -= 1; }
            }
            else
            {
                t_entry = t_max_y;
                t_max_y += t_delta_y;
                cell_y += step_y;
                local_cell_y += step_y;
                if (local_cell_y == cells_per_chunk) { local_cell_y = 0; chunk_off_y += 1; }
                else if (local_cell_y == -1) { local_cell_y = cells_per_chunk - 1; chunk_off_y -= 1; }
            }
        }
        return true;
    };
    if constexpr (HasObjectPool)
    {
        for (bool done = false; !done; )
            done = walk.template operator()<u16>() || walk.template operator()<u32>();
    }
    else
        walk.template operator()<u16>();

    if constexpr(EnableDiagnostics)
        diag.tmin = b ? 0 : radius > 0 ? min_tmin
                  : ray_aabb_intersection(Vector2(from_px), dir_inv_norm,
                                          {{Vector2(best_px) - Vector2{fuzz2}, Vector2(best_px) + Vector2{fuzz2}}},
                                          signs).tmin;
    result.success = b;
    return result;
}

template raycast_result_s do_raycasting<false, false>(std::nullptr_t, world&, point, point, object_id,
                                                      Grid::Pass::Pool&, std::nullptr_t, const Search::pred&, float);
template raycast_result_s do_raycasting<false, true>(std::nullptr_t, world&, point, point, object_id,
                                                     Grid::Pass::Pool&, Grid::Object::Pool&, const Search::pred&, float);
template raycast_result_s do_raycasting<true, false>(raycast_diag_s&, world&, point, point, object_id,
                                                     Grid::Pass::Pool&, std::nullptr_t, const Search::pred&, float);
template raycast_result_s do_raycasting<true, true>(raycast_diag_s&, world&, point, point, object_id,
                                                    Grid::Pass::Pool&, Grid::Object::Pool&, const Search::pred&, float);

template void traverse_line_cells(world&, chunk_coords_, const bresenham_line&, u32, u32, bool,
                                  std::nullptr_t, const Search::pred&, std::array<Array<bool>, 4>*,
                                  function_view<bool(const line_cell&) const>,
                                  object_cell_visitor<u16>, object_cell_visitor<u32>);
template void traverse_line_cells(world&, chunk_coords_, const bresenham_line&, u32, u32, bool,
                                  Grid::Pass::Pool*, const Search::pred&, std::array<Array<bool>, 4>*,
                                  function_view<bool(const line_cell&) const>,
                                  object_cell_visitor<u16>, object_cell_visitor<u32>);
template void traverse_line_cells(world&, chunk_coords_, const bresenham_line&, u32, u32, bool,
                                  Grid::Object::Pool*, const Search::pred&, std::array<Array<bool>, 4>*,
                                  function_view<bool(const line_cell&) const>,
                                  object_cell_visitor<u16>, object_cell_visitor<u32>);

} // namespace floormat::Raycast::detail
