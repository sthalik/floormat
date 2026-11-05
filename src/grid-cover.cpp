#include "grid-cover.hpp"
#include "grid.inl"
#include "intra-coord.inl"
#include "grid-pass.hpp"
#include "object.hpp"
#include "world.hpp"
#include "raycast-loop.hpp"
#include "point.inl"
#include "compat/assume-aligned.hpp"
#include "compat/function2.hpp"
#include "compat/nanosecond.inl"
#include "compat/timer.hpp"
#include <bit>
#include <cstring>
#include <array>
#include <cr/Memory.h>
#include <mg/Functions.h>

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#endif
#include <hwy/highway.h>

namespace floormat::detail::grid {

namespace {

using Grid::Cover::Params;
using Grid::Cover::octant_count;

// Octant k steps octant_steps[k] cells at a time, within 0.7° of k*11.25°, so a fill can carry a
// distance from a cell to the cell a step away.
constexpr std::array<Vector2b, octant_count> octant_steps = [] {
    constexpr std::array<std::array<i8, 2>, 8> quadrant = {{
        {1, 0}, {5, 1}, {5, 2}, {3, 2}, {1, 1}, {2, 3}, {2, 5}, {1, 5},
    }};
    std::array<Vector2b, octant_count> ret{};
    for (u32 k = 0; k < octant_count; k++)
    {
        auto [x, y] = quadrant[k % 8];
        for (u32 i = 0; i < k / 8; i++)
        {
            const auto x0 = x;
            x = (i8)-y;
            y = x0;
        }
        ret[k] = Vector2b{x, y};
    }
    return ret;
}();

constexpr bool is_aligned(Vector2b step)
{
    return step.x() == 0 || step.y() == 0 || step.x() == step.y() || step.x() == -step.y();
}

Vector2 direction_for_octant(u32 k)
{
    return Vector2{octant_steps[k]}.normalized();
}

struct crossed_cell
{
    i8 x, y;
    u8 major;
};

struct stepped_octant
{
    i8 q, p;
    u8 count;
    // Cells the segment between the centers of a cell and the cell a step away passes through,
    // ends left out, farthest first. Both cells meeting at a corner on the segment count.
    std::array<crossed_cell, 8> cells;
};

constexpr stepped_octant make_stepped_octant(Vector2b step)
{
    const int q = step.x(), p = step.y();
    const int aq = q < 0 ? -q : q, ap = p < 0 ? -p : p;
    stepped_octant ret{(i8)q, (i8)p, 0, {}};
    if (is_aligned(step))
        return ret;
    // In doubled coordinates the segment runs from (0, 0) to (2q, 2p) and cell (i, j) spans 2i - 1
    // to 2i + 1 and 2j - 1 to 2j + 1. A cell within the segment's bounding box is crossed when its
    // corners aren't all strictly on one side of the line.
    for (int j = p < 0 ? p : 0; j <= (p < 0 ? 0 : p); j++)
        for (int i = q < 0 ? q : 0; i <= (q < 0 ? 0 : q); i++)
        {
            if ((i == 0 && j == 0) || (i == q && j == p))
                continue;
            bool below = false, above = false;
            for (int cy = 2*j - 1; cy <= 2*j + 1; cy += 2)
                for (int cx = 2*i - 1; cx <= 2*i + 1; cx += 2)
                {
                    const int side = p*cx - q*cy;
                    below |= side <= 0;
                    above |= side >= 0;
                }
            if (!below || !above)
                continue;
            fm_assert(ret.count < ret.cells.size());
            const int major = aq > ap ? (i < 0 ? -i : i) : (j < 0 ? -j : j);
            ret.cells[ret.count++] = {(i8)i, (i8)j, (u8)major};
        }
    for (u32 a = 1; a < ret.count; a++)
        for (u32 b = a; b > 0 && ret.cells[b - 1].major < ret.cells[b].major; b--)
            std::swap(ret.cells[b - 1], ret.cells[b]);
    return ret;
}

constexpr std::array<stepped_octant, octant_count> stepped_octants = [] {
    std::array<stepped_octant, octant_count> ret{};
    for (u32 k = 0; k < octant_count; k++)
        ret[k] = make_stepped_octant(octant_steps[k]);
    return ret;
}();

constexpr u32 ring_rows = [] {
    u32 n = 0;
    for (auto s : octant_steps)
    {
        const auto rows = (u32)(s.y() < 0 ? -s.y() : s.y()) + 1;
        n = rows > n ? rows : n;
    }
    return n;
}();

constexpr std::array<u8, octant_count> octant_order = {
    0, 8, 16, 24,
    4, 12, 20, 28,
    1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15,
    17, 18, 19, 21, 22, 23, 25, 26, 27, 29, 30, 31,
};

constexpr std::array<Vector2b, 16> ring2_offsets = {{
    {-2, -2}, {-1, -2}, { 0, -2}, { 1, -2}, { 2, -2},
    {-2, -1},                               { 2, -1},
    {-2,  0},                               { 2,  0},
    {-2,  1},                               { 2,  1},
    {-2,  2}, {-1,  2}, { 0,  2}, { 1,  2}, { 2,  2},
}};

// fill_aligned() carries pixels in 1/fixed_one px in u16. Carries saturate at UINT16_MAX,
// which rounds to dc, the cap.
constexpr u32 fixed_one = (UINT16_MAX + 1) / chunk_size_xy; // xxx todo assumes power of two chunk size

u32 to_fixed(float px)
{
    return Math::min((u32)(px * (float)fixed_one + .5f), (u32)UINT16_MAX);
}

u32 fixed_add(u32 a, u32 b)
{
    return Math::min(a + b, (u32)UINT16_MAX);
}

template<typename T>
T fixed_to_units(u32 fixed, u32 div_size)
{
    const auto unit = div_size * fixed_one;
    return (T)((fixed + unit / 2) / unit);
}

// Pixels from the center of cell (cell_x, cell_y) to the first hit on the way to the center plus
// delta, at most max_ray_px. Calls do_raycasting() itself, since raycast() reads the clock twice
// per ray.
float raycast_one(chunk& self,
                  u32 cell_x, u32 cell_y,
                  u32 div_size, Vector2i delta,
                  Pass::Pool& pass_pool,
                  u32 max_ray_px = chunk_size_xy)
{
    fm_assert(div_size > 0);
    fm_assert(chunk_size_xy % div_size == 0);
    fm_debug_assert(cell_x < chunk_size_xy / div_size);
    fm_debug_assert(cell_y < chunk_size_xy / div_size);

    const auto half = (i32)(div_size / 2);
    const Vector2i origin_local{
        (i32)(cell_x * div_size) + half,
        (i32)(cell_y * div_size) + half,
    };

    const point from{self.coord(), intra_coord{origin_local}};
    const point to = from + delta;

    const auto r = rc::detail::do_raycasting<false>(nullptr, self.world(), from, to, 0, pass_pool,
                                                    Search::without_critters(), 0);

    if (!r.has_result || r.success)
        return (float)max_ray_px;
    return Math::min(Vector2(r.collision - from).length(), (float)max_ray_px);
}

// Bit n of the pass grid is bit (n & 7) of byte (n >> 3), as BitArray stores it.
bool pass_bit(const u8* bits, u32 n)
{
    return bits[n >> 3] >> (n & 7) & 1;
}

// One row of a vertical or diagonal octant: a blocked cell stores (T)-1 and carries 0, and a passable
// one is a step further than prev[x + sx]. prev[-1] and prev[dc] must be readable, and the caller
// redoes the edge cell that reads one. Returns false and touches nothing when the row doesn't split
// into whole vectors.
template<u32 DivSize, typename T>
bool sweep_row_vectorized(u16* prev, T* out, const u8* row_bits, i32 sx,
                          u32 step, u32 runtime_div_size)
{
    const u32 div_size = DivSize ? DivSize : runtime_div_size, dc = chunk_size_xy / div_size;
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr hn::ScalableTag<u16> d;
    constexpr hn::Rebind<u8, decltype(d)> d8;
    const auto N = (u32)hn::Lanes(d);
    // whole vectors, each starting on a byte of the pass grid
    if (N % 8 != 0 || dc % N != 0)
        return false;

    // Rows are dc cells apart, and dc is a multiple of N, so of 8.
    constexpr u32 row_bytes = (DivSize ? chunk_size_xy / DivSize : 8) * sizeof *out;
    constexpr u32 out_align = row_bytes < 64 ? row_bytes : 64; // xxx todo assumes power of two chunk size
    prev = assume_aligned<64>(prev);
    out = assume_aligned<out_align>(out);

    // LoadMaskBits() may read 8 bytes, which runs past the end of the grid on its last row
    alignas(64) u8 bits[chunk_size_xy / 8 + 8];
    std::memcpy(bits, row_bits, dc / 8);

    const auto vstep = hn::Set(d, (u16)step);
    const auto vone = hn::Set(d, u16{1});
    const auto vblocked = hn::Set(d, (u16)(T)-1);
    // px/unit rounded as (px/(unit/2) + 1)/2, since px + unit/2 can overflow
    const auto half_shift = std::countr_zero(div_size * fixed_one) - 1; // xxx todo assumes power of two chunk size
    const auto dx = sx < 0 ? 0u - N : N;
    for (u32 b = 0, x = sx < 0 ? dc - N : 0; b < dc; b += N, x += dx)
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
bool sweep_row(u16* prev, T* out, const u8* row_bits, i32 sx, u32 step,
               u32 div_size)
{
    static_assert(chunk_size_xy / 4 >= UINT8_MAX && chunk_size_xy / 8 < UINT8_MAX,
                  "u8 grids start at div_size 8");
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

// whole vectors, each starting on a byte of the pass grid
bool stepped_rows_vectorize(u32 dc)
{
    namespace hn = hwy::HWY_NAMESPACE;
    const auto N = (u32)hn::Lanes(hn::ScalableTag<u16>{});
    return N % 8 == 0 && dc % N == 0;
}

// One u16 per cell, all ones where passable, so a vector can load from any cell.
void expand_pass_row(u16* out, const u8* row_bits, u32 dc)
{
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr hn::ScalableTag<u16> d;
    const auto N = (u32)hn::Lanes(d);
    // LoadMaskBits() may read 8 bytes, which runs past the end of the grid on its last row
    alignas(64) u8 bits[chunk_size_xy / 8 + 8];
    std::memcpy(bits, row_bits, dc / 8);
    for (u32 x = 0; x < dc; x += N)
        hn::StoreU(hn::VecFromMask(d, hn::LoadMaskBits(d, bits + x / 8)), d, out + x);
}

// One row of a stepped octant, as fill_stepped() describes it. next is the row |p| back in sweep
// order and masks[j] the pass masks of the row j back. A cell whose cell a step away is off the
// grid reads spare cells of next and masks, so the caller redoes it.
template<typename T>
void sweep_stepped_row(const u16* next, u16* cur, const u16* const* masks, T* out,
                       const stepped_octant& o, u32 step, const u16* stop_px,
                       u32 div_size)
{
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr hn::ScalableTag<u16> d;
    constexpr hn::Rebind<u8, decltype(d)> d8;
    const auto N = (u32)hn::Lanes(d);
    const u32 dc = chunk_size_xy / div_size;

    const auto vstep = hn::Set(d, (u16)step);
    const auto vone = hn::Set(d, u16{1});
    const auto vblocked = hn::Set(d, (u16)(T)-1);
    const auto half_shift = std::countr_zero(div_size * fixed_one) - 1; // xxx todo assumes power of two chunk size
    for (u32 x = 0; x < dc; x += N)
    {
        auto px = hn::SaturatedAdd(hn::LoadU(d, next + ((isize)x + o.q)), vstep);
        // farthest first, so the nearest blocked cell wins
        for (u32 c = 0; c < o.count; c++)
        {
            const auto& cell = o.cells[c];
            const auto* m = masks[cell.y < 0 ? -cell.y : cell.y] + ((isize)x + cell.x);
            px = hn::IfThenElse(hn::MaskFromVec(hn::LoadU(d, m)), px, hn::Set(d, stop_px[c]));
        }
        const auto passable = hn::MaskFromVec(hn::LoadU(d, masks[0] + x));
        px = hn::IfThenElseZero(passable, px);
        hn::StoreU(px, d, cur + x);
        const auto rounded = hn::ShiftRight<1>(hn::Add(hn::ShiftRightSame(px, half_shift), vone));
        const auto units = hn::IfThenElse(passable, rounded, vblocked);
        if constexpr (sizeof(T) == 1)
            hn::StoreU(hn::DemoteTo(d8, units), d8, out + x);
        else
            hn::StoreU(units, d, out + x);
    }
}

constexpr u32 line_cells = 64 / sizeof(u16);

u32 round_up(u32 n, u32 multiple)
{
    return (n + multiple - 1) / multiple * multiple;
}

u32 row_size(u32 dc)
{
    return round_up(dc, line_cells) + 2*line_cells;
}

} // namespace

struct CoverGrid : GridBase
{
    using Params = Grid::Cover::Params;

    Params params;
    u32 cell_count;
    // A distance is at most dc, and (T)-1 is Grid::Cover::blocked, so u8 needs dc < UINT8_MAX.
    bool narrow;
    // cell_count rounded up, so every plane starts on 64 bytes
    u32 plane_stride;
    // Units of params.div_size, as u8 when narrow. Octant k of cell i is at
    // k*plane_stride + i, so a fill writes one contiguous plane. Planes not in built_octants hold
    // stale data.
    Array<u16> distances;
    // fill_aligned()'s previous row in pixels, or fill_stepped()'s ring_rows rows of pixels and as
    // many of pass masks. Rows are row_size() apart, starting at row[line_cells], on 64 bytes, with
    // spare cells either side.
    Array<u16> row;
    u32 built_octants = 0;

    CoverGrid(chunk& c, Params params);
    ~CoverGrid() noexcept = default;

    void reset_for_reuse(chunk& ch, Params new_params);
    void apply_params();
    // A ray reads the pass grids and RTrees of the 3×3 around the chunk, and those hold objects of
    // the next ring out. Only maybe_mark_stale() calls this, as check_current() runs on reads.
    bool ring2_is_current() const;
    void maybe_mark_stale();
    template<typename T> T* octant_plane(u32 k);

    static u32 get_cell_index(u32 x, u32 y, u32 div_count);
    u32 get_cell_index_from_coord(intra_coord pos) const;

    void build_impl(chunk* self);
    bool fill_octant(u32 k, chunk& self);
    template<typename T> void fill_aligned(u32 k, chunk& self, const u8* bits, Vector2 dir,
                                           i32 sx, i32 sy, float step_px);
    template<typename T> void fill_stepped(u32 k, chunk& self, const u8* bits);
    bool fill_next_unfilled(chunk& self);
};

struct CoverPool : Pool<CoverGrid>
{
    Pass::Pool& pass_pool;
    u32 fills = 0;
    Ns fill_time{};

    CoverPool(Params p, Pass::Pool& pass_pool);
};

CoverPool::CoverPool(Params p, Pass::Pool& pass_pool):
    Pool<CoverGrid>{p},
    pass_pool{pass_pool}
{
    fm_assert(pass_pool.params() == Pass::Params{params.div_size, pass_mode::shoot_through});
}

u32 CoverGrid::get_cell_index(u32 x, u32 y, u32 div_count)
{
    return GridBase::pack_bit_index(x, y, div_count);
}

u32 CoverGrid::get_cell_index_from_coord(intra_coord pos) const
{
    const auto dc = chunk_size_xy / params.div_size;
    return GridBase::pack_bit_index_from_coord(pos, dc);
}

void CoverGrid::build_impl(chunk* self)
{
    // else a rebuild serves the previous build's octants 1..31 until re-filled
    built_octants = 0;
    fill_octant(0, *self);
}

bool CoverGrid::ring2_is_current() const
{
    const auto& w = c->world();
    for (auto off : ring2_offsets)
        if (const auto* ch = w.at(coord + off); ch && ch->pass_gen() > built_pass_gen)
            return false;
    return true;
}

void CoverGrid::maybe_mark_stale()
{
    fm_debug3_assert(c && c->world().at(coord) == c);

    if (!is_stale() && (!is_current() || !ring2_is_current()))
        mark_stale();
}

bool CoverGrid::fill_octant(u32 k, chunk& self)
{
    fm_debug_assert(k < octant_count);
    if (built_octants & (1u << k))
        return false;

    const u32 div_size = params.div_size;
    auto& P = *static_cast<CoverPool*>(pool);
    auto& pass_pool = P.pass_pool;
    fm_assert(pass_pool.params().div_size() == div_size);
    pass_pool.maybe_mark_stale_all(self.world());
    auto t = Time::now();

    const u32 dc = chunk_size_xy / div_size;

    auto pass_grid = pass_pool[self];
    pass_grid.build_if_stale();
    // pass bit and cell (x, y) are both y*dc + x, as pack_bit_index() lays them out
    fm_debug_assert(get_cell_index(1, 1, dc) == dc + 1);
    const u8* const bits = pass_grid.bits().data;

    const auto s = octant_steps[k];
    if (is_aligned(s))
    {
        const auto dir = direction_for_octant(k);
        const i32 sx = s.x() > 0 ? 1 : s.x() < 0 ? -1 : 0;
        const i32 sy = s.y() > 0 ? 1 : s.y() < 0 ? -1 : 0;
        const auto step_px = (float)div_size / (sx ? Math::abs(dir.x()) : Math::abs(dir.y()));
        if (narrow)
            fill_aligned<u8>(k, self, bits, dir, sx, sy, step_px);
        else
            fill_aligned<u16>(k, self, bits, dir, sx, sy, step_px);
    }
    else if (narrow)
        fill_stepped<u8>(k, self, bits);
    else
        fill_stepped<u16>(k, self, bits);

    built_octants |= (1u << k);
    const auto dt = t.update();
    P.fills++;
    P.fill_time += dt;
#if 0
    DBG << "{fill octant}" << coord << "k=" << k
        << "took" << (int)(Time::to_milliseconds(dt) * 1e3f) << "us";
#endif
    return true;
}

// An aligned octant steps a whole cell at a time, so a passable cell is one step further than the
// next one along the octant. Only a cell whose next one is off the grid casts a ray.
template<typename T>
void CoverGrid::fill_aligned(u32 k, chunk& self, const u8* bits, Vector2 dir,
                             i32 sx, i32 sy, float step_px)
{
    const u32 div_size = params.div_size, dc = chunk_size_xy / div_size;
    auto& pass_pool = static_cast<CoverPool*>(pool)->pass_pool;
    T* const plane = octant_plane<T>(k);

    const Vector2i delta{Vector2(dir * (float)chunk_size_xy)};
    const auto ray_fixed = [&](u32 x, u32 y) {
        return to_fixed(raycast_one(self, x, y, div_size, delta, pass_pool));
    };
    // The n-th step in sweep order is the difference of rounded multiples, so a carry over any
    // run of steps is off by at most 1/fixed_one px.
    const auto step_at = [step = (double)step_px * fixed_one](u32 n) {
        return (u32)(n*step + .5) - (u32)((n - 1)*step + .5);
    };

    if (sy == 0)
    {
        u16 steps[chunk_size_xy];
        for (u32 n = 1; n < dc; n++)
            steps[n] = (u16)step_at(n);
        // rows don't depend on each other, so carry the next cell's pixels along each one
        for (u32 y = 0; y < dc; y++)
        {
            u32 px = 0;
            for (u32 n = 0; n < dc; n++)
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
    u16* const prev = row.data() + line_cells;
    const auto scalar_cell = [&](u32 n, u32 x, u32 y) {
        const auto i = y*dc + x;
        // wraps past dc at x = 0 when sx < 0
        const auto nx = x + (u32)sx;
        u32 px = 0;
        if (pass_bit(bits, i))
        {
            px = n == 0 || nx >= dc ? ray_fixed(x, y) : fixed_add(prev[nx], step_at(n));
            plane[i] = fixed_to_units<T>(px, div_size);
        }
        else
            plane[i] = (T)-1;
        prev[x] = (u16)px;
    };

    for (u32 n = 0; n < dc; n++)
    {
        const auto y = sy > 0 ? dc - 1 - n : n;
        if (n > 0 && sweep_row(prev, plane + y*dc, bits + y*dc/8, sx, step_at(n), div_size))
        {
            // the cell whose next one is off the grid read row's spare cell instead
            if (sx != 0)
                scalar_cell(n, sx > 0 ? dc - 1 : 0, y);
            continue;
        }
        for (u32 m = 0; m < dc; m++)
            scalar_cell(n, sx < 0 ? dc - 1 - m : m, y);
    }
}

// The other octants step (q, p) cells at a time, sweeping rows from the edge p points at. A passable
// cell whose cell a step away is off the grid casts a ray. Else the nearest blocked cell that the
// segment between their centers crosses stops it, at that cell's center along the major axis, or
// else it is a step further than the cell a step away, which carries 0 when blocked.
template<typename T>
void CoverGrid::fill_stepped(u32 k, chunk& self, const u8* bits)
{
    const u32 div_size = params.div_size, dc = chunk_size_xy / div_size;
    auto& pass_pool = static_cast<CoverPool*>(pool)->pass_pool;
    T* const plane = octant_plane<T>(k);
    const auto& o = stepped_octants[k];
    const i32 q = o.q, p = o.p;
    const u32 aq = (u32)Math::abs(q), ap = (u32)Math::abs(p);
    const double len = Math::sqrt((double)(q*q + p*p));

    // A ray along whole steps runs through the centers of the cells that carry its distance.
    const auto delta = Vector2i{q, p} * (i32)Math::ceil(chunk_size_xy / len);
    const auto ray_fixed = [&](u32 x, u32 y) {
        return to_fixed(raycast_one(self, x, y, div_size, delta, pass_pool));
    };
    // R(n) is n/|p| steps rounded, and row n of the sweep adds R(n) - R(n - |p|), so a carry over
    // any run of steps is off by at most 1/fixed_one px.
    const double row_fixed = div_size * len * fixed_one / ap;
    const auto R = [row_fixed](u32 n) { return (u32)(n*row_fixed + .5); };
    std::array<u16, std::tuple_size_v<decltype(o.cells)>> stop_px;
    for (u32 c = 0; c < o.count; c++)
    {
        const double major_px = o.cells[c].major * div_size * len / Math::max(aq, ap);
        stop_px[c] = (u16)Math::min((u32)(major_px*fixed_one + .5), (u32)UINT16_MAX);
    }

    const u32 slots = ap + 1, rs = row_size(dc);
    fm_debug_assert(slots <= ring_rows);
    u16* const carries = row.data() + line_cells;
    u16* const masks = carries + ring_rows*rs;
    const auto carry_row = [&](u32 n) { return carries + n % slots * rs; };
    const auto mask_row = [&](u32 n) { return masks + n % slots * rs; };

    const auto scalar_cell = [&](u32 n, u32 x, u32 y, u32 step) {
        const auto i = y*dc + x;
        // wraps past dc when x + q < 0
        const auto nx = x + (u32)q;
        u32 px = 0;
        if (pass_bit(bits, i))
        {
            if (n < ap || nx >= dc)
                px = ray_fixed(x, y);
            else
            {
                px = fixed_add(carry_row(n - ap)[nx], step);
                for (auto c = (u32)o.count; c-- > 0; )
                {
                    const auto& cell = o.cells[c];
                    if (!pass_bit(bits, (y + (u32)cell.y)*dc + x + (u32)cell.x))
                    {
                        px = stop_px[c];
                        break;
                    }
                }
            }
            plane[i] = fixed_to_units<T>(px, div_size);
        }
        else
            plane[i] = (T)-1;
        carry_row(n)[x] = (u16)px;
    };

    const bool vectorize = stepped_rows_vectorize(dc) && aq < dc;
    std::array<const u16*, ring_rows> row_masks;
    for (u32 n = 0; n < dc; n++)
    {
        const auto y = p > 0 ? dc - 1 - n : n;
        const auto step = n < ap ? 0 : Math::min(R(n) - R(n - ap), (u32)UINT16_MAX);
        if (vectorize)
            expand_pass_row(mask_row(n), bits + y*dc/8, dc);
        if (vectorize && n >= ap)
        {
            for (u32 j = 0; j <= ap; j++)
                row_masks[j] = mask_row(n - j);
            sweep_stepped_row(carry_row(n - ap), carry_row(n), row_masks.data(), plane + y*dc, o, step,
                              stop_px.data(), div_size);
            for (u32 m = 0; m < aq; m++)
                scalar_cell(n, q > 0 ? dc - 1 - m : m, y, step);
            continue;
        }
        for (u32 x = 0; x < dc; x++)
            scalar_cell(n, x, y, step);
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
    const u32 width = narrow ? 1 : 2;
    plane_stride = round_up(cell_count, 64 / width);
    const auto size = octant_count * plane_stride * width / sizeof(u16);
    if (distances.size() < size) [[unlikely]]
        distances = Utility::allocateAligned<u16, 64>(NoInit, size);
    if (row.size() < 2*ring_rows*row_size(dc)) [[unlikely]]
        row = Utility::allocateAligned<u16, 64>(ValueInit, 2*ring_rows*row_size(dc));
}

template<typename T>
T* CoverGrid::octant_plane(u32 k)
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
    // reads a pass pool of the same div_size, see Pass::Params::validate()
    fm_assert(div_size < chunk_size_xy);
    fm_assert(div_size <= max_div_size);
    return *this;
}

bool Params::operator==(const Params&) const noexcept = default;

Vector2 octant_direction(u32 k)
{
    fm_debug_assert(k < octant_count);
    return detail::grid::direction_for_octant(k);
}
Grid::~Grid() noexcept = default;
Grid::Grid(const Grid&) noexcept = default;
Grid& Grid::operator=(const Grid&) & noexcept = default;

Grid::Grid(detail::grid::CoverGrid* grid_): grid{grid_} {}

u32 Grid::get_cell_index(u32 x, u32 y, u32 div_count)
{
    return detail::grid::CoverGrid::get_cell_index(x, y, div_count);
}

u32 Grid::get_cell_index_from_coord(intra_coord pos) const
{
    return grid->get_cell_index_from_coord(pos);
}

u16 Grid::distance(u32 index, u32 octant) const
{
    fm_debug_assert(octant < octant_count);
    fm_debug_assert(index < grid->cell_count);
    fm_assert(grid->built_octants & 1u << octant);
    if (grid->narrow)
    {
        const u8 d = grid->octant_plane<u8>(octant)[index];
        return d == UINT8_MAX ? blocked : d;
    }
    else
        return grid->octant_plane<u16>(octant)[index];
}

u32 Grid::div_count() const { return chunk_size_xy / grid->params.div_size; }
u64 Grid::build_no() const { return grid->build_no; }

Grid::operator bool() const noexcept { return grid != nullptr; }
detail::grid::CoverGrid* Grid::raw() const noexcept { return grid; }

bool Grid::is_stale() const { return grid->is_stale(); }
void Grid::mark_stale() { grid->mark_stale(); }
void Grid::maybe_mark_stale() { grid->maybe_mark_stale(); }

void Grid::build_if_stale()
{
    grid->build_if_stale();
}

bool Grid::ensure_octant(u32 k)
{
    fm_debug_assert(k < octant_count);
    grid->build_if_stale();
    return grid->fill_octant(k, *grid->c);
}

bool Grid::fill_next_unfilled()
{
    detail::grid::check_current(grid);
    grid->build_if_stale();
    return grid->fill_next_unfilled(*grid->c);
}

u32 Grid::built_octants() const
{
    detail::grid::check_current(grid);
    return grid->built_octants;
}

Grid Pool::operator[](chunk& c)
{
    return Grid{detail::grid::pool_subscript(pool, c)};
}

void Pool::maybe_mark_stale_all(const world& w)
{
    if (!pool->update_checked_pass_gen(w))
        return;
    for (auto* g : pool->live)
        static_cast<detail::grid::CoverGrid*>(g)->maybe_mark_stale();
}

void Pool::build_if_stale_all()
{
    for (auto* g : pool->live)
        static_cast<detail::grid::CoverGrid*>(g)->build_if_stale();
}

Pool::Pool(Params params, Pass::Pool& pass_pool): pool{new detail::grid::CoverPool{params, pass_pool}} { }
Pool::~Pool() noexcept { delete pool; }
Params Pool::params() const { return pool->params; }
u32 Pool::pooled_count() const { return pool->freelist.size(); }
u32 Pool::octant_fills() const { return pool->fills; }
Ns Pool::octant_fill_time() const { return pool->fill_time; }

} // namespace floormat::Grid::Cover
