#include "grid-cover.hpp"
#include "grid.inl"
#include "intra-coord.inl"
#include "grid-pass.hpp"
#include "object.hpp"
#include "world.hpp"
#include "raycast.hpp"
#include "point.inl"
#include "compat/function2.hpp"
#include <bit>
#include <cfloat>
#include <cstring>
#include <array>
#include <cr/Array.h>
#include <mg/Functions.h>
#include <mg/Timeline.h>
#include <hwy/highway.h>

namespace floormat::detail::grid {

namespace {

using Cover::Params;
using Cover::octant_count;

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

uint16_t raycast_one(chunk& self,
                    uint32_t cell_x, uint32_t cell_y,
                    uint32_t div_size, uint32_t octant,
                    Pass::Pool& pass_pool,
                    uint32_t max_ray_px = chunk_size_xy)
{
    fm_assert(div_size > 0);
    fm_assert(chunk_size_xy % div_size == 0);
    fm_debug_assert(octant < octant_count);
    fm_debug_assert(cell_x < chunk_size_xy / div_size);
    fm_debug_assert(cell_y < chunk_size_xy / div_size);

    const auto half = (int32_t)(div_size / 2);
    const Vector2i origin_local{
        (int32_t)(cell_x * div_size) + half,
        (int32_t)(cell_y * div_size) + half,
    };

    const point from = intra_coord{origin_local}.to_point(self.coord());

    const auto dir = direction_for_octant(octant);
    const Vector2i delta{Vector2(dir * (float)max_ray_px)};
    const point to = from + delta;

    const auto r = raycast(self.world(), from, to, 0, pass_pool, Search::without_critters());

    uint32_t dist_px;
    if (!r.has_result || r.success)
        dist_px = max_ray_px;
    else
        dist_px = (uint32_t)Vector2(r.collision - from).length();

    return (uint16_t)(Math::min(dist_px, max_ray_px) / div_size);
}

// Bit n of the pass grid is bit (n & 7) of byte (n >> 3), as BitArray stores it.
bool pass_bit(const uint8_t* bits, uint32_t n)
{
    return bits[n >> 3] >> (n & 7) & 1;
}

// One row of a vertical or diagonal octant: a blocked cell is 0, and a passable one is a step
// further than the previous row's cell at x + sx. Blocks go against sx, so a block reads
// prev[x + sx] before the block next to it overwrites it. prev[-1] and prev[dc] must be
// readable; the caller redoes the edge cell that reads one of them. Returns false when the row
// doesn't split into whole vectors, and touches nothing.
bool sweep_row_vectorized(uint16_t* prev, uint16_t* out, const uint8_t* row_bits, uint32_t dc,
                          int32_t sx, uint32_t step, uint32_t div_size)
{
    namespace hn = hwy::HWY_NAMESPACE;
    const hn::ScalableTag<uint16_t> d;
    const auto N = (uint32_t)hn::Lanes(d);
    // whole vectors, each starting on a byte of the pass grid
    if (!std::has_single_bit(uint32_t{chunk_size_xy}) || N % 8 != 0 || dc % N != 0)
        return false;

    // LoadMaskBits() may read 8 bytes, which runs past the end of the grid on its last row
    uint8_t bits[chunk_size_xy / 8 + 8];
    std::memcpy(bits, row_bits, dc / 8);

    const auto vstep = hn::Set(d, (uint16_t)step);
    const auto vmax = hn::Set(d, (uint16_t)chunk_size_xy);
    const auto shift = std::countr_zero(div_size);
    for (uint32_t b = 0; b < dc; b += N)
    {
        const auto x = sx < 0 ? dc - N - b : b;
        const auto next = hn::Add(hn::LoadU(d, prev + x + sx), vstep);
        const auto px = hn::IfThenElseZero(hn::LoadMaskBits(d, bits + x / 8), next);
        hn::StoreU(px, d, prev + x);
        hn::StoreU(hn::ShiftRightSame(hn::Min(px, vmax), shift), d, out + x);
    }
    return true;
}

} // namespace

struct CoverGrid : GridBase
{
    using Params = Grid::Cover::Params;

    Params params;
    uint32_t cell_count;
    // Units of params.div_size. Octant k of cell i is at k*cell_count + i, so a fill writes one
    // contiguous plane. Planes not in built_octants hold stale data.
    Array<uint16_t> distances;
    // fill_aligned()'s previous row in pixels, from row[1], with a spare cell either side
    Array<uint16_t> row;
    uint32_t built_octants = 0;

    CoverGrid(chunk& c, Params params);
    ~CoverGrid() noexcept = default;

    void reset_for_reuse(chunk& ch, Params new_params);

    static uint32_t get_cell_index(uint32_t x, uint32_t y, uint32_t div_count);
    uint32_t get_cell_index_from_coord(intra_coord pos) const;

    void build_impl(chunk* self);
    bool fill_octant(uint32_t k, chunk& self);
    void fill_aligned(uint32_t k, chunk& self, int32_t sx, int32_t sy, uint32_t step);
    bool fill_next_unfilled(chunk& self);
};

struct CoverPool : Pool<CoverGrid>
{
    Pass::Pool pass_pool;

    explicit CoverPool(Params p);
};

CoverPool::CoverPool(Params p):
    Pool<CoverGrid>{p},
    pass_pool{Pass::Params{params.div_size, pass_mode::shoot_through}}
{
}

uint32_t CoverGrid::get_cell_index(uint32_t x, uint32_t y, uint32_t div_count)
{
    return GridBase::pack_bit_index(x, y, div_count);
}

uint32_t CoverGrid::get_cell_index_from_coord(intra_coord pos) const
{
    const auto dc = chunk_size_xy / params.div_size;
    return GridBase::pack_bit_index_from_coord(pos, params.div_size, dc);
}

void CoverGrid::build_impl(chunk* self)
{
    // else a rebuild serves the previous build's octants 1..31 until re-filled
    built_octants = 0;
    fill_octant(0, *self);

    for (auto i = 0u; i < 8; i++)
        versions[i] = neighbors[i] ? neighbors[i]->pass_gen() : (uint64_t)-1;
    versions[8] = self->pass_gen();
}

bool CoverGrid::fill_octant(uint32_t k, chunk& self)
{
    fm_debug_assert(k < octant_count);
    if (built_octants & (1u << k))
        return false;

    const uint32_t div_size = params.div_size;
    auto& pass_pool = static_cast<CoverPool*>(pool)->pass_pool;
    fm_assert(pass_pool.params().div_size == div_size);
    pass_pool.maybe_mark_stale_all(self.world().frame_no());
    Timeline timeline;
    timeline.start();

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
        // round: truncating div_size*sqrt2 loses ~2.8% per diagonal step
        const auto step = (uint32_t)((sx == 0 ? t_delta_y : t_delta_x) + .5f);
        fill_aligned(k, self, sx, sy, step);
    }
    else
    {
        uint16_t* const plane = distances.data() + k*cell_count;
        for (uint32_t cy = 0; cy < dc; ++cy)
            for (uint32_t cx = 0; cx < dc; ++cx)
                plane[get_cell_index(cx, cy, dc)] = raycast_one(self, cx, cy, div_size, k, pass_pool);
    }

    built_octants |= (1u << k);
#if 0
    DBG << "{fill octant}" << coord << "k=" << k
        << "took" << (int)(timeline.currentFrameDuration() * 1e6f) << "us";
#endif
    return true;
}

// At most a ray across the chunk plus a diagonal across it of rounded steps, each off by at most
// half a pixel: chunk_size_xy*(1 + sqrt2 + 1/2).
static_assert(3*chunk_size_xy <= 65535, "fill_aligned() keeps pixels in uint16_t");

// An aligned octant steps a whole cell at a time, so a passable cell is one rounded step further
// than the next cell along the octant, and a blocked one is 0. Rows are swept starting from the
// edge the octant points at, and only a cell whose next one is off the grid casts a ray.
void CoverGrid::fill_aligned(uint32_t k, chunk& self, int32_t sx, int32_t sy, uint32_t step)
{
    const uint32_t div_size = params.div_size, dc = chunk_size_xy / div_size;
    auto& pass_pool = static_cast<CoverPool*>(pool)->pass_pool;
    auto pass_grid = pass_pool[self];
    pass_grid.build_if_stale(Search::without_critters());

    // pass bit and cell (x, y) are both y*dc + x, as pack_bit_index() lays them out
    fm_debug_assert(get_cell_index(1, 1, dc) == dc + 1);
    const uint8_t* const bits = pass_grid.bits().data;
    uint16_t* const plane = distances.data() + k*cell_count;

    const auto ray_px = [&](uint32_t x, uint32_t y) {
        return (uint32_t)raycast_one(self, x, y, div_size, k, pass_pool) * div_size;
    };
    const auto units = [div_size](uint32_t px) {
        px = Math::min(px, chunk_size_xy);
        if constexpr (std::has_single_bit(uint32_t{chunk_size_xy}))
            return (uint16_t)(px >> std::countr_zero(div_size));
        else
            return (uint16_t)(px / div_size);
    };

    if (sy == 0)
    {
        // rows don't depend on each other, so carry the next cell's pixels along each one
        for (uint32_t y = 0; y < dc; y++)
        {
            uint32_t px = 0;
            for (uint32_t n = 0; n < dc; n++)
            {
                const auto x = sx > 0 ? dc - 1 - n : n;
                const auto i = y*dc + x;
                if (!pass_bit(bits, i))
                    px = 0;
                else
                    px = n == 0 ? ray_px(x, y) : px + step;
                plane[i] = units(px);
            }
        }
        return;
    }

    // Each row needs the one before it in sweep order. Walking x against sx reads row[x + sx]
    // before this row overwrites it.
    uint16_t* const prev = row.data() + 1;
    const auto scalar_cell = [&](uint32_t n, uint32_t x, uint32_t y) {
        const auto i = y*dc + x;
        // wraps past dc at x = 0 when sx < 0
        const auto nx = x + (uint32_t)sx;
        uint32_t px = 0;
        if (pass_bit(bits, i))
            px = n == 0 || nx >= dc ? ray_px(x, y) : prev[nx] + step;
        prev[x] = (uint16_t)px;
        plane[i] = units(px);
    };

    for (uint32_t n = 0; n < dc; n++)
    {
        const auto y = sy > 0 ? dc - 1 - n : n;
        if (n > 0 && sweep_row_vectorized(prev, plane + y*dc, bits + y*dc/8, dc, sx, step, div_size))
        {
            // the cell whose next one is off the grid read row's spare cell instead
            if (sx != 0)
                scalar_cell(n, sx > 0 ? dc - 1 : 0, y);
            continue;
        }
        for (uint32_t m = 0; m < dc; m++)
            scalar_cell(n, sx < 0 ? dc - 1 - m : m, y);
    }
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
    cell_count{(chunk_size_xy / params.div_size) * (chunk_size_xy / params.div_size)},
    distances{NoInit, octant_count * cell_count},
    row{ValueInit, chunk_size_xy / params.div_size + 2}
{
}

void CoverGrid::reset_for_reuse(chunk& ch, Params new_params)
{
    reset_base_for_reuse(ch);
    params = new_params;
    built_octants = 0;
    const auto dc = chunk_size_xy / params.div_size;
    cell_count = dc * dc;
    if (distances.size() < octant_count * cell_count) [[unlikely]]
        distances = Array<uint16_t>{NoInit, octant_count * cell_count};
    if (row.size() < dc + 2) [[unlikely]]
        row = Array<uint16_t>{ValueInit, dc + 2};
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

uint32_t Grid::get_cell_index_from_coord(intra_coord pos) const
{
    detail::grid::check_frame_sync(pool, grid);
    return grid->get_cell_index_from_coord(pos);
}

uint16_t Grid::distance(uint32_t index, uint32_t octant) const
{
    detail::grid::check_frame_sync(pool, grid);
    fm_debug_assert(octant < octant_count);
    fm_debug_assert(index < grid->cell_count);
    // an octant that isn't built yet reads as 0
    if (!(grid->built_octants & 1u << octant))
        return 0;
    return grid->distances[octant*grid->cell_count + index];
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
    return grid->fill_octant(k, *grid->c);
}

bool Grid::fill_next_unfilled()
{
    detail::grid::check_frame_sync(pool, grid);
    grid->build_if_stale();
    return grid->fill_next_unfilled(*grid->c);
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
    for (auto* g : pool->live)
        g->maybe_mark_stale();
}

void Pool::build_if_stale_all()
{
    for (auto* g : pool->live)
        static_cast<detail::grid::CoverGrid*>(g)->build_if_stale();
}

Pool::Pool(Params params): pool{new detail::grid::CoverPool{params}} { }
Pool::~Pool() noexcept { delete pool; }
Params Pool::params() const { return pool->params; }
uint64_t Pool::frame_no() const { return pool->frame_no; }
uint32_t Pool::pooled_count() const { return pool->freelist.size(); }

} // namespace floormat::Grid::Cover
