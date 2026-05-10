#include "raycast-diag.hpp"
#include "tile-constants.hpp"
#include "pass-mode.hpp"
#include "world.hpp"
#include "grid-pass.hpp"
#include "search-pred.hpp"
#include "search.hpp"
#include "RTree-search.hpp"
#include "compat/function2.hpp"
#include <cfloat>
#include <bit>
#include <cr/StructuredBindings.h>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Timeline.h>

namespace floormat::rc {

namespace {

template<typename T> constexpr inline auto tile_size = Math::Vector2<T>{iTILE_SIZE2};
template<typename T> constexpr inline auto chunk_size = Math::Vector2<T>{TILE_MAX_DIM} * tile_size<T>;

using floormat::detail_rc::bbox;
using RTree = std::decay_t<decltype(*std::declval<class chunk>().rtree())>;
using Rect = typename RTree::Rect;

static_assert(tile_size<int>.x() == tile_size<int>.y());

constexpr Vector2 pt_to_vec(point from, point pt)
{
    auto V = Vector2{};
    V += (Vector2(pt.chunk()) - Vector2(from.chunk())) * chunk_size<float>;
    V += (Vector2(pt.local()) - Vector2(from.local())) * tile_size<float>;
    V += (Vector2(pt.offset()) - Vector2(from.offset()));
    return V;
}

struct aabb_result
{
    float tmin;
    bool result;
};

template<typename T>
std::array<uint8_t, 2> ray_aabb_signs(Math::Vector2<T> ray_dir_inv_norm)
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

        tmin = max(dmin, tmin);
        tmax = min(dmax, tmax);
    }

    return { tmin, tmin < tmax };
}

template<typename T>
constexpr bool within_chunk_bounds(Math::Vector2<T> p0, Math::Vector2<T> p1)
{
    constexpr auto max_bb_size = Math::Vector2<T>{T{0xff}, T{0xff}};
    constexpr auto half_bb = (max_bb_size + Math::Vector2{T{1}}) / T{2};
    constexpr auto start = -half_bb, end = chunk_size<T> + half_bb;

    return start.x() <= p1.x() && end.x() >= p0.x() &&
           start.y() <= p1.y() && end.y() >= p0.y();
}

template bool within_chunk_bounds<int>(Math::Vector2<int> p0, Math::Vector2<int> p1);

template<bool EnableDiagnostics>
raycast_result_s do_raycasting(std::conditional_t<EnableDiagnostics, raycast_diag_s&, std::nullptr_t> diag,
                               world& w, point from, point to, object_id self,
                               Grid::Pass::Pool& pool, const Search::pred& pred)
{
    raycast_result_s result;
    fm_assert(from.chunk3().z == to.chunk3().z);

    using Math::abs;
    using Math::floor;

    constexpr auto inv_eps = 1e6f, eps = 1/inv_eps;
    constexpr auto fuzz2 = 0.5f;

    result.has_result = false;

    auto V = pt_to_vec(from, to);
    auto ray_len = V.length();

    if (ray_len < eps)
    {
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
    auto dir_inv_norm = Vector2{1} / dir;
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

    if (pool.frame_no() != w.frame_no())
        pool.maybe_mark_stale_all(w.frame_no());
    const auto& bit_pred = Search::never_continue();

    constexpr auto half_tile_i = tile_size<int>.x() / 2;
    const auto div_size_i = (int32_t)pool.params().div_size;
    const auto div_size_f = (float)div_size_i;
    const auto cells_per_chunk = chunk_size<int>.x() / div_size_i;

    Vector2 from_shifted {
        (float)(from.local().x * tile_size<int>.x() + from.offset().x() + half_tile_i),
        (float)(from.local().y * tile_size<int>.y() + from.offset().y() + half_tile_i),
    };

    int32_t cell_x = (int32_t)floor(from_shifted.x() / div_size_f);
    int32_t cell_y = (int32_t)floor(from_shifted.y() / div_size_f);

    int32_t chunk_off_x, local_cell_x;
    if (cell_x >= 0)
    {
        chunk_off_x = cell_x / cells_per_chunk;
        local_cell_x = cell_x - chunk_off_x * cells_per_chunk;
    }
    else
    {
        chunk_off_x = -((cells_per_chunk - 1 - cell_x) / cells_per_chunk);
        local_cell_x = cell_x - chunk_off_x * cells_per_chunk;
    }
    int32_t chunk_off_y, local_cell_y;
    if (cell_y >= 0)
    {
        chunk_off_y = cell_y / cells_per_chunk;
        local_cell_y = cell_y - chunk_off_y * cells_per_chunk;
    }
    else
    {
        chunk_off_y = -((cells_per_chunk - 1 - cell_y) / cells_per_chunk);
        local_cell_y = cell_y - chunk_off_y * cells_per_chunk;
    }

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
        diag = {
            .V = V,
            .dir = dir,
            .dir_inv_norm = dir_inv_norm,
            .tmin = 0,
        };
        auto chunk_count_max = (uint32_t)(abs(V.x()) / (float)chunk_size<int>.x()
                                        + abs(V.y()) / (float)chunk_size<int>.y()) + 2u;
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
    bool b = true;
    float t_entry = 0;

    while (t_entry <= ray_len + eps && b)
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
                    last_nb[j*3 + i] = w.chunk_at_memo(co);
                }
            last_c = last_nb[4];
            last_g_built = false;

            if constexpr(EnableDiagnostics)
            {
                chunk_coords_ ch_coord {
                    (int16_t)(from.chunk().x + chunk_off_x),
                    (int16_t)(from.chunk().y + chunk_off_y),
                    from.chunk3().z };
                point chunk_origin{ch_coord, local_coords{0, 0}, Vector2b{0, 0}};
                auto chunk_center = point::normalize_coords(chunk_origin, Vector2i{
                    chunk_size<int>.x() / 2 - half_tile_i,
                    chunk_size<int>.y() / 2 - half_tile_i,
                });
                arrayAppend(diag.path, bbox{
                    chunk_center,
                    Vector2ui{(uint32_t)chunk_size<int>.x(), (uint32_t)chunk_size<int>.y()},
                });
            }
        }

        bool bit = true;
        if (last_c)
        {
            if (!last_g_built)
            {
                last_g = pool[*last_c];
                last_g.build_if_stale(bit_pred);
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
            last_chunk_empty = true;

        if (!bit)
        {
            if constexpr(EnableDiagnostics)
            {
                chunk_coords_ q_ch_coord {
                    (int16_t)(from.chunk().x + chunk_off_x),
                    (int16_t)(from.chunk().y + chunk_off_y),
                    from.chunk3().z };
                point chunk_origin{q_ch_coord, local_coords{0, 0}, Vector2b{0, 0}};
                auto cell_center = point::normalize_coords(chunk_origin, Vector2i{
                    local_cell_x * div_size_i,
                    local_cell_y * div_size_i,
                });
                arrayAppend(diag.queries, bbox{
                    cell_center,
                    Vector2ui{(uint32_t)(div_size_i + 1), (uint32_t)(div_size_i + 1)},
                });
            }

            float cell_min_x_f = (float)cell_x * div_size_f - (float)half_tile_i - fuzz2;
            float cell_max_x_f = (float)(cell_x + 1) * div_size_f - (float)half_tile_i + fuzz2;
            float cell_min_y_f = (float)cell_y * div_size_f - (float)half_tile_i - fuzz2;
            float cell_max_y_f = (float)(cell_y + 1) * div_size_f - (float)half_tile_i + fuzz2;

            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    auto* nb = last_nb[j*3 + i];
                    if (!nb)
                        continue;
                    int32_t nb_off_x = chunk_off_x + i - 1;
                    int32_t nb_off_y = chunk_off_y + j - 1;

                    float nb_world_x = (float)(nb_off_x * chunk_size<int>.x());
                    float nb_world_y = (float)(nb_off_y * chunk_size<int>.y());

                    Vector2 fmin{cell_min_x_f - nb_world_x, cell_min_y_f - nb_world_y};
                    Vector2 fmax{cell_max_x_f - nb_world_x, cell_max_y_f - nb_world_y};

                    if (!within_chunk_bounds(fmin, fmax))
                        continue;

                    Vector2 origin {
                        (float)(from.local().x * tile_size<int>.x() + from.offset().x()) - nb_world_x,
                        (float)(from.local().y * tile_size<int>.y() + from.offset().y()) - nb_world_y,
                    };

                    nb->rtree()->Search(fmin.data(), fmax.data(), [&](uint64_t data, const Rect& r)
                    {
                        auto x = std::bit_cast<collision_data>(data);
                        if (x.id == self || x.pass == (uint64_t)pass_mode::pass)
                            return true;
                        auto range = Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}};
                        if (pred(*nb, x, range) == path_search_continue::pass)
                            return true;
                        auto ret = ray_aabb_intersection(origin, dir_inv_norm,
                                                         {{{r.m_min[0]-fuzz2, r.m_min[1]-fuzz2},
                                                           {r.m_max[0]+fuzz2, r.m_max[1]+fuzz2}}},
                                                         signs);
                        if (!ret.result)
                            return true;
                        if (ret.tmin > ray_len) [[unlikely]]
                            return true;
                        if (ret.tmin < min_tmin) [[likely]]
                        {
                            min_tmin = ret.tmin;
                            result.collision = point::normalize_coords(from, Vector2i(dir * min_tmin));
                            result.collider = x;
                            b = false;
                        }
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
        diag.tmin = b ? 0 : min_tmin;
    result.success = b;
    return result;
}

} // namespace

raycast_result_s raycast(world& w, point from, point to, object_id self,
                         Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred)
{
    Timeline timeline;
    timeline.start();
    auto ret = do_raycasting<false>(nullptr, w, from, to, self, pass_grid_pool, pred);
    ret.time = timeline.currentFrameDuration();
    return ret;
}

raycast_result_s raycast(world& w, point from, point to, object_id self)
{
    return raycast(w, from, to, self, w.raycast_pass_pool(), Search::never_continue());
}

raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self,
                                   Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred)
{
    Timeline timeline;
    timeline.start();
    auto ret = do_raycasting<true>(diag, w, from, to, self, pass_grid_pool, pred);
    ret.time = timeline.currentFrameDuration();
    return ret;
}

raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self)
{
    return raycast_with_diag(diag, w, from, to, self, w.raycast_pass_pool(), Search::never_continue());
}

} // namespace floormat::rc
