#include "grid-cover.hpp"
#include "grid.inl"
#include "grid-pass.hpp"
#include "object.hpp"
#include "world.hpp"
#include "raycast.hpp"
#include "point.inl"
#include "compat/function2.hpp"
#include <cfloat>
#include <array>
#include <cr/Array.h>
#include <mg/Functions.h>
#include <mg/Timeline.h>
#include <gtl/phmap.hpp>

namespace floormat::detail::grid {

struct CoverCell
{
    // distance units are params.div_size, so a 1-chunk-range ray (=64 at div=16)
    // fits in 6 bits.
    uint8_t distance[Cover::octant_count];
};

namespace {

static_assert(sizeof(CoverCell) == Cover::octant_count);

using Cover::Params;
using Cover::octant_count;

constexpr auto can_shoot_through_lambda = [](chunk& self, collision_data data) {
    if (data.pass == (uint64_t)pass_mode::shoot_through)
        return path_search_continue::pass;
    if (data.type == (uint64_t)collision_type::scenery)
    {
        auto obj = self.world().find_object(data.id);
        fm_assert(obj);
        if (obj->type() == object_type::critter)
            return path_search_continue::pass;
    }
    return path_search_continue::blocked;
};
constexpr Search::pred can_shoot_through{can_shoot_through_lambda};

Vector2 direction_for_octant(uint32_t k)
{
    using namespace floormat;
    constexpr float step = 2.f * Math::Constants<float>::pi() / (float)octant_count;
    const auto theta = Rad{(float)k * step};
    return {Math::cos(theta), Math::sin(theta)};
}

constexpr std::array<uint8_t, octant_count> octant_order = {
    0, 8, 16, 24,
    4, 12, 20, 28,
    1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15,
    17, 18, 19, 21, 22, 23, 25, 26, 27, 29, 30, 31,
};

uint8_t raycast_one(chunk& self,
                    uint32_t cell_x, uint32_t cell_y,
                    uint32_t div_size, uint32_t octant,
                    Pass::Pool& pass_pool,
                    uint32_t max_ray_px = chunk_size_xy)
{
    fm_assert(div_size > 0);
    fm_assert(chunk_size_xy % div_size == 0);
    fm_assert(pass_pool.params().div_size == div_size);
    fm_debug_assert(octant < octant_count);
    fm_debug_assert(cell_x < chunk_size_xy / div_size);
    fm_debug_assert(cell_y < chunk_size_xy / div_size);

    const auto half = (int32_t)(div_size / 2);
    const Vector2i origin_local{
        (int32_t)(cell_x * div_size) + half,
        (int32_t)(cell_y * div_size) + half,
    };

    const point chunk_nw{
        self.coord(),
        local_coords{0, 0},
        Vector2b{(int8_t)(-tile_size_xy/2), (int8_t)(-tile_size_xy/2)},
    };
    const point from = chunk_nw + origin_local;

    const auto dir = direction_for_octant(octant);
    const Vector2i delta{Vector2(dir * (float)max_ray_px)};
    const point to = from + delta;

    const auto r = raycast(self.world(), from, to, 0, pass_pool, can_shoot_through);

    uint32_t dist_px;
    if (!r.has_result || r.success)
        dist_px = max_ray_px;
    else
        dist_px = (uint32_t)Vector2(r.collision - from).length();

    const auto units = Math::min(dist_px, max_ray_px) / div_size;
    return (uint8_t)Math::min<uint32_t>(units, 255);
}

} // namespace

struct CoverGrid : GridBase
{
    using Params = Grid::Cover::Params;

    Params params;
    Array<CoverCell> cells;
    uint32_t built_octants = 0;

    CoverGrid(chunk& c, Params params);
    ~CoverGrid() noexcept = default;

    void reset_for_reuse(chunk& ch, Params new_params);

    static uint32_t get_cell_index(uint32_t x, uint32_t y, uint32_t div_count);
    uint32_t get_cell_index_from_coord(local_coords local, Vector2b offset) const;

    void build_impl(chunk* self);
    bool fill_octant(uint32_t k, chunk& self);
    bool fill_next_unfilled(chunk& self);
};

uint32_t CoverGrid::get_cell_index(uint32_t x, uint32_t y, uint32_t div_count)
{
    return GridBase::pack_bit_index(x, y, div_count);
}

uint32_t CoverGrid::get_cell_index_from_coord(local_coords local, Vector2b offset) const
{
    const auto dc = chunk_size_xy / params.div_size;
    return GridBase::pack_bit_index_from_coord(local, offset, params.div_size, dc);
}

void CoverGrid::build_impl(chunk* self)
{
    built_octants = 0;
    fill_octant(0, *self);

    for (auto i = 0u; i < 8; i++)
        versions[i] = neighbors[i] ? neighbors[i]->pass_gen_counter() : (uint32_t)-1;
    versions[8] = self->pass_gen_counter();
}

bool CoverGrid::fill_octant(uint32_t k, chunk& self)
{
    fm_debug_assert(k < octant_count);
    if (built_octants & (1u << k))
        return false;

    auto& pass_pool = w->cover_pass_pool();
    pass_pool.maybe_mark_stale_all(w->frame_no());
    Timeline timeline;
    timeline.start();

    const uint32_t div_size = params.div_size;
    const uint32_t dc = chunk_size_xy / div_size;

    const auto dir = direction_for_octant(k);
    const float ax = Math::abs(dir.x()), ay = Math::abs(dir.y());
    const int32_t sx = ax < 1e-6f ? 0 : dir.x() > 0 ? 1 : -1;
    const int32_t sy = ay < 1e-6f ? 0 : dir.y() > 0 ? 1 : -1;
    const bool aligned = (sx == 0) || (sy == 0)
                      || Math::abs(ax - ay) < 1e-4f;

    if (aligned)
    {
        const float t_delta_x = ax > 1e-6f ? (float)div_size / ax : FLT_MAX;
        const float t_delta_y = ay > 1e-6f ? (float)div_size / ay : FLT_MAX;

        enum step_kind { sk_x, sk_y, sk_diag };
        step_kind sk;
        float step_t;
        if (sx == 0)         { sk = sk_y;    step_t = t_delta_y; }
        else if (sy == 0)    { sk = sk_x;    step_t = t_delta_x; }
        else                 { sk = sk_diag; step_t = t_delta_x; }

        auto pass_grid = pass_pool[self];
        pass_grid.build_if_stale(can_shoot_through);

        Array<uint16_t> px_dist{ValueInit, dc * dc};

        const int32_t dc_i = (int32_t)dc;
        for (int32_t y_idx = 0; y_idx < dc_i; ++y_idx)
        {
            const int32_t cy = sy > 0 ? dc_i - 1 - y_idx : y_idx;
            for (int32_t x_idx = 0; x_idx < dc_i; ++x_idx)
            {
                const int32_t cx = sx > 0 ? dc_i - 1 - x_idx : x_idx;
                const uint32_t idx = get_cell_index((uint32_t)cx, (uint32_t)cy, dc);

                const auto bit_idx = Pass::Grid::get_bitmask_index((uint32_t)cx, (uint32_t)cy, dc);
                const bool passable = pass_grid.bit(bit_idx);

                uint32_t dist_px;
                if (!passable)
                    dist_px = 0;
                else
                {
                    int32_t nx, ny;
                    switch (sk)
                    {
                    case sk_x:    nx = cx + sx; ny = cy;      break;
                    case sk_y:    nx = cx;      ny = cy + sy; break;
                    case sk_diag: nx = cx + sx; ny = cy + sy; break;
                    }

                    if (nx >= 0 && nx < dc_i && ny >= 0 && ny < dc_i)
                    {
                        const uint32_t n_idx = get_cell_index((uint32_t)nx, (uint32_t)ny, dc);
                        dist_px = (uint32_t)px_dist[n_idx] + (uint32_t)step_t;
                    }
                    else
                    {
                        const auto u = raycast_one(self, (uint32_t)cx, (uint32_t)cy, div_size, k, pass_pool);
                        dist_px = (uint32_t)u * div_size;
                    }
                }

                dist_px = Math::min<uint32_t>(dist_px, 65535u);
                px_dist[idx] = (uint16_t)dist_px;

                const uint32_t units = Math::min<uint32_t>(dist_px, chunk_size_xy) / div_size;
                cells[idx].distance[k] = (uint8_t)Math::min<uint32_t>(units, 255);
            }
        }
    }
    else
    {
        for (uint32_t cy = 0; cy < dc; ++cy)
            for (uint32_t cx = 0; cx < dc; ++cx)
            {
                const auto idx = get_cell_index(cx, cy, dc);
                cells[idx].distance[k] = raycast_one(self, cx, cy, div_size, k, pass_pool);
            }
    }

    built_octants |= (1u << k);
#if 0
    DBG << "{fill octant}" << coord << "k=" << k
        << "took" << (int)(timeline.currentFrameDuration() * 1e6f) << "us";
#endif
    return true;
}

bool CoverGrid::fill_next_unfilled(chunk& self)
{
    for (auto k : octant_order)
        if (!(built_octants & (1u << k)))
            return fill_octant(k, self);
    return false;
}

CoverGrid::CoverGrid(chunk& c, Params params):
    GridBase{c},
    params{params},
    cells{ValueInit, (chunk_size_xy / params.div_size) * (chunk_size_xy / params.div_size)}
{
}

void CoverGrid::reset_for_reuse(chunk& ch, Params new_params)
{
    reset_base_for_reuse(ch);
    params = new_params;
    built_octants = 0;
    const auto dc = chunk_size_xy / params.div_size;
    const auto count = dc * dc;
    if (cells.size() < count) [[unlikely]]
        cells = Array<CoverCell>{ValueInit, count};
    else
        for (auto& cc : cells)
            for (auto& d : cc.distance) d = 0;
}

template struct Pool<CoverGrid>;

} // namespace floormat::detail::grid

namespace floormat::Grid::Cover {

Params Params::validate() const
{
    fm_assert(div_size > 0);
    fm_assert(chunk_size_xy % div_size == 0);
    return *this;
}

bool Params::operator==(const Params&) const noexcept = default;
Grid::~Grid() noexcept = default;
Grid::Grid(const Grid&) noexcept = default;
Grid& Grid::operator=(const Grid&) & noexcept = default;

Grid::Grid(detail::grid::CoverGrid* grid_, detail::grid::Pool<detail::grid::CoverGrid>* pool_):
    grid{grid_}, pool{pool_}
{
}

uint32_t Grid::get_cell_index(uint32_t x, uint32_t y, uint32_t div_count)
{
    return detail::grid::CoverGrid::get_cell_index(x, y, div_count);
}

uint32_t Grid::get_cell_index_from_coord(local_coords local, Vector2b offset) const
{
    detail::grid::check_frame_sync(pool, grid);
    return grid->get_cell_index_from_coord(local, offset);
}

const detail::grid::CoverCell& Grid::cell(uint32_t index) const
{
    detail::grid::check_frame_sync(pool, grid);
    fm_debug_assert(index < grid->cells.size());
    return grid->cells[index];
}

uint8_t Grid::distance(uint32_t index, uint32_t octant) const
{
    fm_debug_assert(octant < octant_count);
    return cell(index).distance[octant];
}

uint32_t Grid::div_count() const { return chunk_size_xy / grid->params.div_size; }
uint64_t Grid::build_no() const { return grid->build_no; }

Grid::operator bool() const noexcept { return grid != nullptr; }
detail::grid::CoverGrid* Grid::raw() const noexcept { return grid; }

void Grid::mark_stale() { grid->mark_stale(); }
void Grid::maybe_mark_stale() { grid->maybe_mark_stale(); }

void Grid::build_if_stale()
{
    grid->build_if_stale();
}

bool Grid::ensure_octant(uint32_t k)
{
    fm_debug_assert(k < octant_count);
    detail::grid::check_frame_sync(pool, grid);
    grid->build_if_stale();
    auto* sc = grid->w->chunk_at_memo(grid->coord);
    fm_assert(sc);
    return grid->fill_octant(k, *sc);
}

bool Grid::fill_next_unfilled()
{
    detail::grid::check_frame_sync(pool, grid);
    grid->build_if_stale();
    auto* sc = grid->w->chunk_at_memo(grid->coord);
    fm_assert(sc);
    return grid->fill_next_unfilled(*sc);
}

uint32_t Grid::built_octants() const
{
    detail::grid::check_frame_sync(pool, grid);
    return grid->built_octants;
}

Grid Pool::operator[](chunk& c)
{
    return Grid{detail::grid::pool_subscript(pool, c), pool};
}

Grid Pool::wrap(detail::grid::CoverGrid* g) const noexcept
{
    return Grid{g, pool};
}

void Pool::maybe_mark_stale_all(uint64_t frame_no)
{
    pool->frame_no = frame_no;
    for (auto it = pool->grids.begin(); it != pool->grids.end(); )
    {
        auto* g = it->second;
        g->maybe_mark_stale();
        if (!g->w->chunk_at_memo(g->coord))
        {
            g->c = nullptr;
            it = pool->grids.erase(it);
            pool->put(g);
        }
        else
            ++it;
    }
}

void Pool::build_if_stale_all()
{
    for (auto& [_, g] : pool->grids)
        g->build_if_stale();
}

Pool::Pool(Params params): pool{new detail::grid::Pool<detail::grid::CoverGrid>{params}} { }
Pool::~Pool() noexcept { delete pool; }
Params Pool::params() const { return pool->params; }
uint64_t Pool::frame_no() const { return pool->frame_no; }
uint32_t Pool::pooled_count() const { return pool->freelist.size(); }

} // namespace floormat::Grid::Cover
