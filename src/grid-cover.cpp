#include "grid-cover.hpp"
#include "grid.inl"
#include "intra-coord.inl"
#include "grid-pass.hpp"
#include "object.hpp"
#include "world.hpp"
#include "raycast.hpp"
#include "point.inl"
#include "compat/assume-aligned.hpp"
#include "compat/function2.hpp"
#include <bit>
#include <cfloat>
#include <cstring>
#include <array>
#include <cr/Array.h>
#include <cr/Memory.h>
#include <mg/Functions.h>
#include <mg/Timeline.h>

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#endif
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

// fill_aligned() carries pixels in 1/fixed_one px in uint16_t. Carries saturate at UINT16_MAX,
// which rounds to dc, the cap.
constexpr uint32_t fixed_one = (UINT16_MAX + 1) / chunk_size_xy;

uint32_t to_fixed(float px)
{
    return Math::min((uint32_t)(px * (float)fixed_one + .5f), (uint32_t)UINT16_MAX);
}

uint32_t fixed_add(uint32_t a, uint32_t b)
{
    return Math::min(a + b, (uint32_t)UINT16_MAX);
}

template<typename T>
T fixed_to_units(uint32_t fixed, uint32_t div_size)
{
    const auto unit = div_size * fixed_one;
    return (T)((fixed + unit / 2) / unit);
}

float raycast_one(chunk& self,
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

    if (!r.has_result || r.success)
        return (float)max_ray_px;
    return Math::min(Vector2(r.collision - from).length(), (float)max_ray_px);
}

// Bit n of the pass grid is bit (n & 7) of byte (n >> 3), as BitArray stores it.
bool pass_bit(const uint8_t* bits, uint32_t n)
{
    return bits[n >> 3] >> (n & 7) & 1;
}

// One row of a vertical or diagonal octant: a blocked cell carries 0 and stores (T)-1, and a passable
// one is a step further than the previous row's cell at x + sx. Blocks go against sx, so a block reads
// prev[x + sx] before the block next to it overwrites it. prev[-1] and prev[dc] must be
// readable; the caller redoes the edge cell that reads one of them. prev starts on 64 bytes, and
// out is a row of a plane that does. Returns false when the row doesn't split into whole vectors,
// and touches nothing. DivSize is div_size when it's known at compile time, else 0.
template<uint32_t DivSize, typename T>
bool sweep_row_vectorized(uint16_t* prev, T* out, const uint8_t* row_bits, int32_t sx,
                          uint32_t step, uint32_t runtime_div_size)
{
    const uint32_t div_size = DivSize ? DivSize : runtime_div_size, dc = chunk_size_xy / div_size;
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr hn::ScalableTag<uint16_t> d;
    constexpr hn::Rebind<uint8_t, decltype(d)> d8;
    const auto N = (uint32_t)hn::Lanes(d);
    // whole vectors, each starting on a byte of the pass grid
    if (!std::has_single_bit(uint32_t{chunk_size_xy}) || N % 8 != 0 || dc % N != 0)
        return false;

    // Rows are dc cells apart, and dc is a multiple of N, so of 8.
    constexpr uint32_t row_bytes = (DivSize ? chunk_size_xy / DivSize : 8) * sizeof *out;
    constexpr uint32_t out_align = row_bytes < 64 ? row_bytes : 64;
    prev = assume_aligned<64>(prev);
    out = assume_aligned<out_align>(out);

    // LoadMaskBits() may read 8 bytes, which runs past the end of the grid on its last row
    alignas(64) uint8_t bits[chunk_size_xy / 8 + 8];
    std::memcpy(bits, row_bits, dc / 8);

    const auto vstep = hn::Set(d, (uint16_t)step);
    const auto vone = hn::Set(d, uint16_t{1});
    const auto vblocked = hn::Set(d, (uint16_t)(T)-1);
    // px/unit rounded as (px/(unit/2) + 1)/2, since px + unit/2 can overflow
    const auto half_shift = std::countr_zero(div_size * fixed_one) - 1;
    const auto dx = sx < 0 ? 0u - N : N;
    for (uint32_t b = 0, x = sx < 0 ? dc - N : 0; b < dc; b += N, x += dx)
    {
        const auto next = hn::SaturatedAdd(hn::LoadU(d, prev + x + sx), vstep);
        const auto passable = hn::LoadMaskBits(d, bits + x / 8);
        const auto px = hn::IfThenElseZero(passable, next);
        hn::StoreU(px, d, prev + x);
        const auto rounded = hn::ShiftRight<1>(hn::Add(hn::ShiftRightSame(px, half_shift), vone));
        const auto units = hn::IfThenElse(passable, rounded, vblocked);
        if constexpr (sizeof(T) == 1)
            hn::StoreU(hn::DemoteTo(d8, units), d8, out + x);
        else
            hn::StoreU(units, d, out + x);
    }
    return true;
}

// A div_size known at compile time turns the copy of the row's bits into a few moves. Others call
// memcpy.
template<typename T>
bool sweep_row(uint16_t* prev, T* out, const uint8_t* row_bits, int32_t sx, uint32_t step,
               uint32_t div_size)
{
    static_assert(chunk_size_xy / 4 >= UINT8_MAX && chunk_size_xy / 8 < UINT8_MAX,
                  "uint8_t grids start at div_size 8");
    if constexpr (sizeof(T) == 2)
        switch (div_size)
        {
        case 1:  return sweep_row_vectorized<1, T>(prev, out, row_bits, sx, step, div_size);
        case 2:  return sweep_row_vectorized<2, T>(prev, out, row_bits, sx, step, div_size);
        case 4:  return sweep_row_vectorized<4, T>(prev, out, row_bits, sx, step, div_size);
        default: return sweep_row_vectorized<0, T>(prev, out, row_bits, sx, step, div_size);
        }
    else
        switch (div_size)
        {
        case 8:  return sweep_row_vectorized<8, T>(prev, out, row_bits, sx, step, div_size);
        case 16: return sweep_row_vectorized<16, T>(prev, out, row_bits, sx, step, div_size);
        case 32: return sweep_row_vectorized<32, T>(prev, out, row_bits, sx, step, div_size);
        case 64: return sweep_row_vectorized<64, T>(prev, out, row_bits, sx, step, div_size);
        default: return sweep_row_vectorized<0, T>(prev, out, row_bits, sx, step, div_size);
        }
}

constexpr uint32_t line_cells = 64 / sizeof(uint16_t);

uint32_t round_up(uint32_t n, uint32_t multiple)
{
    return (n + multiple - 1) / multiple * multiple;
}

uint32_t row_size(uint32_t dc)
{
    return round_up(dc, line_cells) + 2*line_cells;
}

} // namespace

struct CoverGrid : GridBase
{
    using Params = Grid::Cover::Params;

    Params params;
    uint32_t cell_count;
    // A distance is at most dc, and (T)-1 is Cover::blocked, so uint8_t needs dc < UINT8_MAX.
    bool narrow;
    // cell_count rounded up, so every plane starts on 64 bytes
    uint32_t plane_stride;
    // Units of params.div_size, as uint8_t when narrow. Octant k of cell i is at
    // k*plane_stride + i, so a fill writes one contiguous plane. Planes not in built_octants hold
    // stale data.
    Array<uint16_t> distances;
    // fill_aligned()'s previous row in pixels. It starts at row[line_cells], on 64 bytes, with spare
    // cells either side.
    Array<uint16_t> row;
    uint32_t built_octants = 0;

    CoverGrid(chunk& c, Params params);
    ~CoverGrid() noexcept = default;

    void reset_for_reuse(chunk& ch, Params new_params);
    void apply_params();
    template<typename T> T* octant_plane(uint32_t k);

    static uint32_t get_cell_index(uint32_t x, uint32_t y, uint32_t div_count);
    uint32_t get_cell_index_from_coord(intra_coord pos) const;

    void build_impl(chunk* self);
    bool fill_octant(uint32_t k, chunk& self);
    template<typename T> void fill_aligned(uint32_t k, chunk& self, const uint8_t* bits,
                                           int32_t sx, int32_t sy, float step_px);
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
    return GridBase::pack_bit_index_from_coord(pos, dc);
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
    fm_assert(pass_pool.params().div_size() == div_size);
    pass_pool.maybe_mark_stale_all(self.world());
    Timeline timeline;
    timeline.start();

    const uint32_t dc = chunk_size_xy / div_size;

    auto pass_grid = pass_pool[self];
    pass_grid.build_if_stale(Search::without_critters());
    // pass bit and cell (x, y) are both y*dc + x, as pack_bit_index() lays them out
    fm_debug_assert(get_cell_index(1, 1, dc) == dc + 1);
    const uint8_t* const bits = pass_grid.bits().data;

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
        const auto step_px = sx == 0 ? t_delta_y : t_delta_x;
        if (narrow)
            fill_aligned<uint8_t>(k, self, bits, sx, sy, step_px);
        else
            fill_aligned<uint16_t>(k, self, bits, sx, sy, step_px);
    }
    else
    {
        const auto fill = [&]<typename T>(T* plane) {
            for (uint32_t cy = 0; cy < dc; ++cy)
                for (uint32_t cx = 0; cx < dc; ++cx)
                {
                    const auto i = get_cell_index(cx, cy, dc);
                    if (!pass_bit(bits, i))
                    {
                        plane[i] = (T)-1;
                        continue;
                    }
                    const auto px = raycast_one(self, cx, cy, div_size, k, pass_pool);
                    plane[i] = fixed_to_units<T>(to_fixed(px), div_size);
                }
        };
        if (narrow)
            fill(octant_plane<uint8_t>(k));
        else
            fill(octant_plane<uint16_t>(k));
    }

    built_octants |= (1u << k);
#if 0
    DBG << "{fill octant}" << coord << "k=" << k
        << "took" << (int)(timeline.currentFrameDuration() * 1e6f) << "us";
#endif
    return true;
}

// An aligned octant steps a whole cell at a time, so a passable cell is one step further than the
// next cell along the octant. A blocked cell carries 0, so the passable cell next to it holds one
// step. Rows are swept starting from the edge the octant points at, and only a cell whose next one
// is off the grid casts a ray.
template<typename T>
void CoverGrid::fill_aligned(uint32_t k, chunk& self, const uint8_t* bits,
                             int32_t sx, int32_t sy, float step_px)
{
    const uint32_t div_size = params.div_size, dc = chunk_size_xy / div_size;
    auto& pass_pool = static_cast<CoverPool*>(pool)->pass_pool;
    T* const plane = octant_plane<T>(k);

    const auto ray_fixed = [&](uint32_t x, uint32_t y) {
        return to_fixed(raycast_one(self, x, y, div_size, k, pass_pool));
    };
    // The n-th step in sweep order is the difference of rounded multiples, so a carry over any
    // run of steps is off by at most 1/fixed_one px.
    const auto step_at = [step = (double)step_px * fixed_one](uint32_t n) {
        return (uint32_t)(n*step + .5) - (uint32_t)((n - 1)*step + .5);
    };

    if (sy == 0)
    {
        uint16_t steps[chunk_size_xy];
        for (uint32_t n = 1; n < dc; n++)
            steps[n] = (uint16_t)step_at(n);
        // rows don't depend on each other, so carry the next cell's pixels along each one
        for (uint32_t y = 0; y < dc; y++)
        {
            uint32_t px = 0;
            for (uint32_t n = 0; n < dc; n++)
            {
                const auto x = sx > 0 ? dc - 1 - n : n;
                const auto i = y*dc + x;
                if (!pass_bit(bits, i))
                {
                    px = 0;
                    plane[i] = (T)-1;
                }
                else
                {
                    px = n == 0 ? ray_fixed(x, y) : fixed_add(px, steps[n]);
                    plane[i] = fixed_to_units<T>(px, div_size);
                }
            }
        }
        return;
    }

    // Each row needs the one before it in sweep order. Walking x against sx reads row[x + sx]
    // before this row overwrites it.
    uint16_t* const prev = row.data() + line_cells;
    const auto scalar_cell = [&](uint32_t n, uint32_t x, uint32_t y) {
        const auto i = y*dc + x;
        // wraps past dc at x = 0 when sx < 0
        const auto nx = x + (uint32_t)sx;
        uint32_t px = 0;
        if (pass_bit(bits, i))
        {
            px = n == 0 || nx >= dc ? ray_fixed(x, y) : fixed_add(prev[nx], step_at(n));
            plane[i] = fixed_to_units<T>(px, div_size);
        }
        else
            plane[i] = (T)-1;
        prev[x] = (uint16_t)px;
    };

    for (uint32_t n = 0; n < dc; n++)
    {
        const auto y = sy > 0 ? dc - 1 - n : n;
        if (n > 0 && sweep_row(prev, plane + y*dc, bits + y*dc/8, sx, step_at(n), div_size))
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
    params{params}
{
    apply_params();
}

void CoverGrid::reset_for_reuse(chunk& ch, Params new_params)
{
    reset_base_for_reuse(ch);
    params = new_params;
    built_octants = 0;
    apply_params();
}

void CoverGrid::apply_params()
{
    const auto dc = chunk_size_xy / params.div_size;
    cell_count = dc * dc;
    narrow = dc < UINT8_MAX;
    const uint32_t width = narrow ? 1 : 2;
    plane_stride = round_up(cell_count, 64 / width);
    const auto size = octant_count * plane_stride * width / sizeof(uint16_t);
    if (distances.size() < size) [[unlikely]]
        distances = Utility::allocateAligned<uint16_t, 64>(NoInit, size);
    if (row.size() < row_size(dc)) [[unlikely]]
        row = Utility::allocateAligned<uint16_t, 64>(ValueInit, row_size(dc));
}

template<typename T>
T* CoverGrid::octant_plane(uint32_t k)
{
    fm_debug_assert(narrow == (sizeof(T) == 1));
    return reinterpret_cast<T*>(distances.data()) + k*plane_stride;
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

Grid::Grid(detail::grid::CoverGrid* grid_): grid{grid_} {}

uint32_t Grid::get_cell_index(uint32_t x, uint32_t y, uint32_t div_count)
{
    return detail::grid::CoverGrid::get_cell_index(x, y, div_count);
}

uint32_t Grid::get_cell_index_from_coord(intra_coord pos) const
{
    detail::grid::check_versions(grid);
    return grid->get_cell_index_from_coord(pos);
}

uint16_t Grid::distance(uint32_t index, uint32_t octant) const
{
    detail::grid::check_versions(grid);
    fm_debug_assert(octant < octant_count);
    fm_debug_assert(index < grid->cell_count);
    fm_assert(grid->built_octants & 1u << octant);
    if (grid->narrow)
    {
        const uint8_t d = grid->octant_plane<uint8_t>(octant)[index];
        return d == UINT8_MAX ? blocked : d;
    }
    else
        return grid->octant_plane<uint16_t>(octant)[index];
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
    detail::grid::check_versions(grid);
    grid->build_if_stale();
    return grid->fill_octant(k, *grid->c);
}

bool Grid::fill_next_unfilled()
{
    detail::grid::check_versions(grid);
    grid->build_if_stale();
    return grid->fill_next_unfilled(*grid->c);
}

uint32_t Grid::built_octants() const
{
    detail::grid::check_versions(grid);
    return grid->built_octants;
}

Grid Pool::operator[](chunk& c)
{
    return Grid{detail::grid::pool_subscript(pool, c)};
}

Grid Pool::wrap(detail::grid::CoverGrid* g) const noexcept
{
    return Grid{g};
}

void Pool::maybe_mark_stale_all(const world& w)
{
    if (!pool->update_checked_pass_gen(w))
        return;
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
uint32_t Pool::pooled_count() const { return pool->freelist.size(); }

} // namespace floormat::Grid::Cover
