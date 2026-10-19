#include "raycast-loop.hpp"
#include "raycast-line.hpp"
#include "raycast-diag.hpp"
#include "tile-constants.hpp"
#include "intra-coord.inl"
#include "point.inl"
#include "pass-mode.hpp"
#include "world.hpp"
#include "grid-pass.hpp"
#include "search-pred.hpp"
#include "search.hpp"
#include "RTree-search.hpp"
#include "compat/function2.hpp"
#include <cfloat>
#include <bit>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat::rc::detail {

namespace {

using floormat::detail_rc::bbox;
using RTree = std::decay_t<decltype(*std::declval<class chunk>().rtree())>;
using Rect = typename RTree::Rect;

} // namespace

Vector2 dir_inverse(Vector2 dir)
{
#if !defined _MSC_VER || defined __clang__
    return Vector2{1} / dir;
#else
    // Under cl's -fp:fast, min/max don't keep the NaN order ray_aabb_intersection relies on.
    // 1/1e-20 still fits after the slab test multiplies it by a distance.
    return Vector2{1} / Math::copysign(Math::max(Math::abs(dir), Vector2{1e-20f}), dir);
#endif
}

std::array<uint8_t, 2> ray_aabb_signs(Vector2 ray_dir_inv_norm)
{
    bool signs[2];
    for (unsigned d = 0; d < 2; ++d)
        signs[d] = std::signbit(ray_dir_inv_norm[d]);
    return { signs[0], signs[1] };
}

// https://tavianator.com/2022/ray_box_boundary.html
// https://www.researchgate.net/figure/The-slab-method-for-ray-intersection-detection-15_fig3_283515372
aabb_result ray_aabb_intersection(Vector2 ray_origin, Vector2 ray_dir_inv_norm,
                                  std::array<Vector2, 2> box_minmax, std::array<uint8_t, 2> signs)
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
                                          std::array<uint8_t, 2> signs)
{
    const auto [bmin, bmax] = box_minmax;
    const auto grown = ray_aabb_intersection(ray_origin, ray_dir_inv_norm,
                                             {{bmin - Vector2{radius}, bmax + Vector2{radius}}}, signs);
    // radius 0 skips the corner test, whose entry point rounding can put outside on both axes
    if (!grown.result || radius == 0)
        return { grown.tmin, grown.result };

    // An entry outside the box on both axes lies in a corner square. Crossing from there
    // into an edge strip passes through the corner's circle, so that circle decides.
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

void traverse_line_cells(world& w, chunk_coords_ ch, const bresenham_line& line,
                         uint32_t first_step, uint32_t last_step, bool both_off_axis,
                         Grid::Pass::Pool* pool, const Search::pred& pred,
                         fu2::function_view<bool(const line_cell&) const> visit)
{
    fm_assert(first_step <= last_step && last_step <= line.steps());
    if (pool)
        pool->maybe_mark_stale_all(w);

    constexpr auto csize = chunk_size<int32_t>;
    const auto div = pool ? (int32_t)pool->params().div_size() : csize;
    fm_debug_assert(std::has_single_bit((uint32_t)div));
    const auto div_count = (uint32_t)(csize / div);
    const auto div_shift = std::countr_zero((uint32_t)div);
    const auto origin = Vector2i{half_tile<int32_t>};
    const auto cell_of = [&](Vector2i p) {
        return Vector2i{(p.x() + origin.x()) >> div_shift, (p.y() + origin.y()) >> div_shift};
    };

    const auto major = line.major_axis(), minor = 1 - major;
    const auto from = line.from();
    const uint64_t len = line.steps(), minor_len = line.minor_len();
    // The steps with minor offset at most b end before t = ceil((2b + 1)*len / (2*minor_len)). Keeping
    // t_rem = t*2*minor_len - (2b + 1)*len lets the next cell's b + div skip the division.
    int64_t t_b = INT32_MIN;
    uint64_t t = 0, t_rem = 0, t_step = 0, t_step_rem = 0;
    if (minor_len)
    {
        t_step = 2*(uint64_t)div*len / (2*minor_len);
        t_step_rem = 2*(uint64_t)div*len % (2*minor_len);
    }
    const auto last_within_minor = [&](int64_t b) -> uint32_t {
        if (b >= (int64_t)minor_len)
            return (uint32_t)len;
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
            const auto a = (2*(uint64_t)b + 1)*len;
            t = (a + d - 1)/d;
            t_rem = t*d - a;
        }
        t_b = b;
        return (uint32_t)(t - 1);
    };

    line_cell cell = {};
    Vector2i loaded{INT32_MAX, INT32_MAX};
    Pass::Grid grid{nullptr, nullptr};
    bool has_grid = false, chunk_empty = false;

    // Callers compare with loaded first, so a cell in the loaded chunk makes no call.
    const auto load = [&](Vector2i off) {
        loaded = off;
        cell.chunk_offset = off;
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++)
                cell.chunks[(size_t)(j*3 + i)] = w.at(chunk_coords_{
                    (int16_t)(ch.x + off.x() + i - 1), (int16_t)(ch.y + off.y() + j - 1), ch.z });
        auto* c = cell.chunks[4];
        has_grid = pool && c;
        if (has_grid)
        {
            grid = (*pool)[*c];
            grid.build_if_stale(pred);
            chunk_empty = grid.is_all_empty();
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
    };

    // v[axis] with a run-time axis goes through the stack.
    const auto on = [](Vector2i v, uint32_t axis) { return axis ? v.y() : v.x(); };
    const auto axis_step = [&](uint32_t axis) {
        const auto s = line.sign(axis);
        return axis ? Vector2i{0, s} : Vector2i{s, 0};
    };
    const auto major_step = axis_step(major), minor_step = axis_step(minor);
    const auto count_shift = std::countr_zero(div_count);
    const auto count_mask = (int32_t)div_count - 1;
    const auto chunk_of = [&](Vector2i c) { return Vector2i{c.x() >> count_shift, c.y() >> count_shift}; };

    // Cells count from the line's chunk, so a cell's chunk and its cell within it are a shift and a mask
    // away. While stepped, step k's cell comes from the previous cell's exit.
    Vector2i global_cell;
    bool stepped = false;
    for (auto k = first_step; ; )
    {
        if (!stepped)
            global_cell = cell_of(line.pixel_at(k));
        const auto chunk_off = chunk_of(global_cell);
        const auto local_cell = Vector2i{global_cell.x() & count_mask, global_cell.y() & count_mask};
        if (chunk_off != loaded)
            load(chunk_off);
        Vector2i lo, hi;
        bool skip = chunk_empty;
        if (chunk_empty)
        {
            lo = chunk_off * csize - origin;
            hi = lo + Vector2i{csize - 1};
        }
        else
        {
            lo = chunk_off * csize + local_cell * div - origin;
            hi = lo + Vector2i{div - 1};
            skip = has_grid && grid.bit(Pass::Grid::get_bitmask_index((uint32_t)local_cell.x(), (uint32_t)local_cell.y(), div_count));
        }
        fm_debug3_assert((line.pixel_at(k) >= lo).all() && (line.pixel_at(k) <= hi).all());
        const auto b = line.sign(minor) > 0 ? (int64_t)on(hi, minor) - on(from, minor) : (int64_t)on(from, minor) - on(lo, minor);
        const auto last_major = line.steps_within(major, on(lo, major), on(hi, major)).last, last_minor = last_within_minor(b);
        const auto last = Math::min(Math::min(last_major, last_minor), last_step);
        if (!skip)
        {
            cell.first_step = k;
            cell.last_step = last;
            if (!visit(cell))
                return;
        }
        if (last == last_step)
            return;

        if (!chunk_empty)
        {
            const bool exit_major = last == last_major, exit_minor = last == last_minor;
            const auto next = global_cell + (exit_major ? major_step : Vector2i{}) + (exit_minor ? minor_step : Vector2i{});
            // Only a step leaving through both sides at once has off-axis pixels outside this cell and the next.
            // Pixel i moves along axis i first, and the minor offset at last is b, as in off_axis_at().
            if (exit_major && exit_minor)
            {
                const auto e = (int64_t)((2*(uint64_t)last + 1)*minor_len) - (int64_t)((2*(uint64_t)b + 1)*len);
                for (auto i = 0u; i < 2; i++)
                {
                    if (!both_off_axis && !(i == major ? e <= 0 : e >= 0))
                        continue;
                    const auto corner = i == 0 ? Vector2i{next.x(), global_cell.y()} : Vector2i{global_cell.x(), next.y()};
                    const auto corner_off = chunk_of(corner);
                    if (corner_off != loaded)
                        load(corner_off);
                    cell.first_step = cell.last_step = last + 1;
                    if (!visit(cell))
                        return;
                }
            }
            global_cell = next;
            stepped = true;
            k = last + 1;
            continue;
        }

        // A diagonal step through the corner of four cells puts its off-axis pixels in two cells
        // neither of its pixels is in. Visited without a bit lookup, since it's rare.
        stepped = false;
        if (const auto o = line.off_axis_at(last); o.diagonal)
        {
            const auto c0 = cell_of(line.pixel_at(last)), c1 = cell_of(line.pixel_at(last + 1));
            for (auto i = 0u; i < 2; i++)
            {
                if (!both_off_axis && !o.crossed[i])
                    continue;
                const auto c = cell_of(o.pixels[i]);
                if (c == c0 || c == c1)
                    continue;
                if (chunk_of(c) != loaded)
                    load(chunk_of(c));
                cell.first_step = cell.last_step = last + 1;
                if (!visit(cell))
                    return;
            }
        }
        k = last + 1;
    }
}

namespace {

struct line_pixel
{
    // 2k for the pixel of step k, 2k + 1 for an off-axis pixel of the step from k to k + 1
    int64_t key;
    Vector2i pixel;
};

// The first pixel of the line's supercover in the closed rect [lo, hi], or bound if its key isn't below it.
line_pixel first_pixel_within(const bresenham_line& line, Vector2i lo, Vector2i hi, int64_t bound)
{
    const auto x = line.steps_within(0, lo.x(), hi.x()), y = line.steps_within(1, lo.y(), hi.y());
    const int64_t x0 = x.first, x1 = x.last, y0 = y.first, y1 = y.last;
    line_pixel ret = { bound, {} };
    if (const auto k = Math::max(x0, y0); k <= Math::min(x1, y1) && 2*k < bound)
        ret = { 2*k, line.pixel_at((uint32_t)k) };
    // The steps putting off-axis pixel i in the rect form a range. If its first step's pixel isn't
    // crossed, a main pixel in the rect comes before every later one.
    for (auto i = 0u; i < 2; i++)
    {
        const auto t  = i == 0 ? Math::max(x0 - 1, y0) : Math::max(x0, y0 - 1);
        const auto t1 = i == 0 ? Math::min(x1 - 1, y1) : Math::min(x1, y1 - 1);
        if (t > Math::min(t1, (int64_t)line.steps() - 1) || 2*t + 1 >= ret.key)
            continue;
        if (const auto o = line.off_axis_at((uint32_t)t); o.diagonal && o.crossed[i])
            ret = { 2*t + 1, o.pixels[i] };
    }
    return ret;
}

} // namespace

template<bool EnableDiagnostics>
raycast_result_s do_raycasting(std::conditional_t<EnableDiagnostics, raycast_diag_s&, std::nullptr_t> diag,
                               world& w, point from, point to, object_id self,
                               Grid::Pass::Pool& pool, const Search::pred& pred, float radius)
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
                .type = (uint64_t)collision_type::none,
                .pass = (uint64_t)pass_mode::pass,
                .id   = ((uint64_t)1 << collision_data_BITS)-1,
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
            .type = (uint64_t)collision_type::none,
            .pass = (uint64_t)pass_mode::pass,
            .id   = ((uint64_t)1 << collision_data_BITS)-1,
        },
        .has_result = true,
        .success = false,
    };

    pool.maybe_mark_stale_all(w);

    const auto params = pool.params();
    fm_assert(radius >= 0);
    fm_assert(min_pool_bbox_size(radius) <= params.bbox_size);
    const auto grow = fuzz2 + radius;
    const auto div_size_i = (int32_t)params.div_size();
    const auto min_pass = (uint64_t)params.min_pass;
    const auto div_size_f = (float)div_size_i;
    const auto cells_per_chunk = (int32_t)params.div_count;

    const auto from_ic = intra_coord{from};
    const auto from_shifted = Vector2(from_ic);
    const auto from_center = Vector2(from_ic.center_shifted());
    const auto from_px = from - point{from.chunk3(), {}, {}};
    const auto line = bresenham_line{from_px, from_px + (to - from)};
    const auto step_len = ray_len / (float)line.steps();

    int32_t cell_x = from_ic.x() / div_size_i, cell_y = from_ic.y() / div_size_i;
    int32_t chunk_off_x = 0, local_cell_x = cell_x;
    int32_t chunk_off_y = 0, local_cell_y = cell_y;

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
        auto chunk_count_max = (uint32_t)(abs(V.x()) / chunk_size<float>
                                        + abs(V.y()) / chunk_size<float>) + 2u;
        arrayClear(diag.path);
        arrayClear(diag.queries);
        arrayReserve(diag.path, chunk_count_max);
        arrayReserve(diag.queries, 8u);
    }

    Vector2i last_chunk_off { (int32_t)1<<30, (int32_t)1<<30 };
    chunk* last_nb[9] = {};
    chunk* last_c = nullptr;
    Pass::Grid last_g{nullptr, nullptr};
    bool last_g_built = false;
    bool last_chunk_empty = true;

    float min_tmin = FLT_MAX;
    int64_t best_key = INT64_MAX;
    Vector2i best_px;
    bool b = true;
    float t_entry = 0;

    // a hit's tmin can lie up to a full rect extent past the cell that found it,
    // so keep walking until the cell entry distance passes the best hit
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
                        (int16_t)(from.chunk().x + chunk_off_x + i - 1),
                        (int16_t)(from.chunk().y + chunk_off_y + j - 1),
                        from.chunk3().z };
                    last_nb[j*3 + i] = w.at(co);
                }
            last_c = last_nb[4];
            last_g_built = false;

            if constexpr(EnableDiagnostics)
            {
                chunk_coords_ ch_coord {
                    (int16_t)(from.chunk().x + chunk_off_x),
                    (int16_t)(from.chunk().y + chunk_off_y),
                    from.chunk3().z };
                arrayAppend(diag.path, bbox{
                    intra_coord{chunk_size<Vector2i> / 2}.to_point(ch_coord),
                    chunk_size<Vector2ui>,
                });
            }
        }

        bool bit = true;
        if (last_c)
        {
            if (!last_g_built)
            {
                last_g = pool[*last_c];
                // a pool holds bitmaps built for one predicate only (see grid-pass.hpp)
                last_g.build_if_stale(pred);
                last_chunk_empty = last_g.is_all_empty();
                last_g_built = true;
            }
            if (!last_chunk_empty)
            {
                auto bit_idx = Pass::Grid::get_bitmask_index(
                    (uint32_t)local_cell_x, (uint32_t)local_cell_y, (uint32_t)cells_per_chunk);
                bit = last_g.bit(bit_idx);
            }
        }
        else
        {
            if (!last_g_built)
            {
                // colliders overhang from loaded neighbors into a missing chunk.
                // skip it only when the whole 3x3 area is unloaded.
                last_chunk_empty = true;
                for (auto* nb : last_nb)
                    last_chunk_empty &= !nb;
                last_g_built = true;
            }
            bit = last_chunk_empty;
        }

        if (!bit)
        {
            if constexpr(EnableDiagnostics)
            {
                chunk_coords_ q_ch_coord {
                    (int16_t)(from.chunk().x + chunk_off_x),
                    (int16_t)(from.chunk().y + chunk_off_y),
                    from.chunk3().z };
                const Vector2i cell{local_cell_x, local_cell_y};
                arrayAppend(diag.queries, bbox{
                    intra_coord{cell * div_size_i + Vector2i{div_size_i / 2}}.to_point(q_ch_coord),
                    Vector2ui{(uint32_t)div_size_i + 1 + (uint32_t)Math::ceil(2*radius)},
                });
            }

            float cell_min_x_f = (float)cell_x * div_size_f - half_tile<float> - grow;
            float cell_max_x_f = (float)(cell_x + 1) * div_size_f - half_tile<float> + grow;
            float cell_min_y_f = (float)cell_y * div_size_f - half_tile<float> - grow;
            float cell_max_y_f = (float)(cell_y + 1) * div_size_f - half_tile<float> + grow;

            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    auto* nb = last_nb[j*3 + i];
                    if (!nb)
                        continue;
                    int32_t nb_off_x = chunk_off_x + i - 1;
                    int32_t nb_off_y = chunk_off_y + j - 1;

                    float nb_world_x = (float)(nb_off_x * chunk_size<int>);
                    float nb_world_y = (float)(nb_off_y * chunk_size<int>);

                    Vector2 fmin{cell_min_x_f - nb_world_x, cell_min_y_f - nb_world_y};
                    Vector2 fmax{cell_max_x_f - nb_world_x, cell_max_y_f - nb_world_y};

                    if (!within_chunk_bounds(fmin, fmax))
                        continue;

                    const auto origin = from_center - Vector2{nb_world_x, nb_world_y};

                    nb->rtree()->Search(fmin.data(), fmax.data(), [&, min_pass](uint64_t data, const Rect& r)
                    {
                        auto x = std::bit_cast<collision_data>(data);
                        fm_debug3_assert(x.type != (uint64_t)collision_type::none);
                        if (x.id == self || x.pass >= min_pass)
                            return true;
                        const auto range = Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}};
                        if (radius == 0)
                        {
                            const auto shift = Vector2i{nb_off_x, nb_off_y} * chunk_size<int32_t>;
                            const auto hit = first_pixel_within(line, Vector2i(Math::ceil(range.min())) + shift,
                                                                Vector2i(Math::floor(range.max())) + shift, best_key);
                            if (hit.key >= best_key) [[likely]]
                                return true;
                            // last, since without_critters looks up the object
                            if (pred(*nb, x, range) == path_search_continue::pass)
                                return true;
                            best_key = hit.key;
                            best_px = hit.pixel;
                            // The ray crosses the square of a pixel with a lower key by step key/2, in a cell
                            // whose query finds its rect. One more step covers t_entry's rounding.
                            min_tmin = (float)(hit.key/2 + 1) * step_len;
                            result.collision = point::normalize_coords(from, hit.pixel - from_px);
                        }
                        else
                        {
                            const auto ret = ray_rounded_rect_intersection(origin, dir, dir_inv_norm,
                                                                           {{range.min() - Vector2{fuzz2},
                                                                             range.max() + Vector2{fuzz2}}},
                                                                           radius, signs);
                            if (!ret.result || ret.tmin > ray_len || ret.tmin >= min_tmin)
                                return true;
                            if (pred(*nb, x, range) == path_search_continue::pass)
                                return true;
                            min_tmin = ret.tmin;
                            const auto contact = Math::clamp(origin + dir * min_tmin, range.min(), range.max());
                            result.collision = point::normalize_coords(from, Vector2i(contact - origin));
                        }
                        result.collider = x;
                        b = false;
                        return true;
                    });
                }
            }
        }

        if (last_chunk_empty)
        {
            int32_t nx = (step_x > 0) ? (cells_per_chunk - local_cell_x)
                       : (step_x < 0) ? (local_cell_x + 1)
                       : INT32_MAX;
            int32_t ny = (step_y > 0) ? (cells_per_chunk - local_cell_y)
                       : (step_y < 0) ? (local_cell_y + 1)
                       : INT32_MAX;

            float t_x_chunk = (step_x == 0) ? FLT_MAX : t_max_x + (float)(nx - 1) * t_delta_x;
            float t_y_chunk = (step_y == 0) ? FLT_MAX : t_max_y + (float)(ny - 1) * t_delta_y;

            if (t_x_chunk <= t_y_chunk)
            {
                int32_t y_crossings = 0;
                if (step_y != 0 && t_max_y <= t_x_chunk)
                    y_crossings = (int32_t)floor((t_x_chunk - t_max_y) / t_delta_y) + 1;
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
                int32_t x_crossings = 0;
                if (step_x != 0 && t_max_x <= t_y_chunk)
                    x_crossings = (int32_t)floor((t_y_chunk - t_max_x) / t_delta_x) + 1;
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

    if constexpr(EnableDiagnostics)
        diag.tmin = b ? 0 : radius > 0 ? min_tmin
                  : ray_aabb_intersection(Vector2(from_px), dir_inv_norm,
                                          {{Vector2(best_px) - Vector2{fuzz2}, Vector2(best_px) + Vector2{fuzz2}}},
                                          signs).tmin;
    result.success = b;
    return result;
}

template raycast_result_s do_raycasting<false>(std::nullptr_t, world&, point, point, object_id,
                                               Grid::Pass::Pool&, const Search::pred&, float);
template raycast_result_s do_raycasting<true>(raycast_diag_s&, world&, point, point, object_id,
                                              Grid::Pass::Pool&, const Search::pred&, float);

} // namespace floormat::rc::detail
