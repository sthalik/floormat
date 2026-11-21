#include "grid-cover.hpp"
#include "grid.inl"
#include "intra-coord.inl"
#include "grid-pass.hpp"
#include "object.hpp"
#include "world.hpp"
#include "raycast-loop.hpp"
#include "point.inl"
#include "compat/assume-aligned.hpp"
#include "compat/function-view.hpp"
#include "compat/map.hpp"
#include "compat/nanosecond.inl"
#include "compat/timer.hpp"
#include <bit>
#include <array>
#include <numeric>
#include <cr/Memory.h>
#include <mg/Functions.h>

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wused-but-marked-unused"
#endif
#undef HWY_DISABLED_TARGETS
#define HWY_DISABLED_TARGETS (HWY_AVX2 - 1)
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "src/grid-cover.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#ifndef FLOORMAT_GRID_COVER_CPP_SHARED
#define FLOORMAT_GRID_COVER_CPP_SHARED

namespace floormat::Grid::detail {

namespace {

using Grid::Cover::Params;
using Grid::Cover::octant_count;

// Octant k's cell offset octant_steps[k] points within 0.7° of k*11.25°, so each cell's distance follows
// from the distance of the cell one offset away.
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

// Returns true when step is horizontal, vertical or at 45°. Graph drawing calls lines made only of such
// segments octilinear.
constexpr bool is_octilinear(Vector2b step)
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

struct non_octilinear_octant
{
    i8 q, p;
    u8 count;
    // Cells the segment between the centers of a cell and the cell one offset away passes through, other than
    // those two cells, farthest first. Both cells meeting at a corner on the segment count.
    std::array<crossed_cell, 8> cells;
};

consteval non_octilinear_octant make_non_octilinear_octant(Vector2b step)
{
    const int q = step.x(), p = step.y();
    const int aq = q < 0 ? -q : q, ap = p < 0 ? -p : p;
    non_octilinear_octant ret{(i8)q, (i8)p, 0, {}};
    if (is_octilinear(step))
        return ret;
    // In doubled coordinates the segment joins (0, 0) and (2q, 2p), and cell (i, j) has corners (2i ± 1, 2j ± 1).
    // A cell in the segment's bounding box is crossed unless its corners all lie strictly on one side of the line.
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

constexpr std::array<non_octilinear_octant, octant_count> non_octilinear_octants =
    map(make_non_octilinear_octant, octant_steps);

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

// While filling, pixel distances are held in u16 as multiples of 1/fixed_one px.
// Sums saturate at UINT16_MAX, which rounds to dc, the cap.
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

// Returns the distance in px from cell (cell_x, cell_y)'s center to the first collider toward the center plus
// delta, at most max_ray_px. It calls do_raycasting() directly, since raycast() reads the clock twice per ray.
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
    // Past the world's edge counts as open, as a missing chunk does. The ray keeps delta's exact direction,
    // so it stops up to 4 px short of the edge, at the last whole step that fits.
    const auto z = self.coord().z;
    const auto room_hi = point{{chunk_xy_max, chunk_xy_max, z}, {TILE_MAX_DIM - 1, TILE_MAX_DIM - 1},
                               Vector2b(half_tile<Vector2i> - Vector2i{1})} - from;
    const auto room_lo = from - point{{chunk_xy_min, chunk_xy_min, z}, {}, Vector2b(-half_tile<Vector2i>)};
    auto n = std::gcd(Math::abs(delta.x()), Math::abs(delta.y()));
    const auto step = n ? delta / n : delta;
    for (auto i = 0u; i < 2; i++)
        if (step[i])
            n = Math::min(n, (step[i] > 0 ? room_hi[i] : room_lo[i]) / Math::abs(step[i]));
    if (n == 0)
        return (float)max_ray_px;
    const point to = from + step*n;

    const auto r = Raycast::detail::do_raycasting<false, false>(nullptr, self.world(), from, to, 0, pass_pool,
                                                                nullptr, Search::without_dynamic_objects, 0);

    if (!r.has_result || r.success)
        return (float)max_ray_px;
    return Math::min(Vector2(r.collision - from).length(), (float)max_ray_px);
}

bool pass_bit(const usize* bits, u32 n)
{
    return bits[n / (sizeof(usize)*8)] >> n % (sizeof(usize)*8) & 1;
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
    // cell_count rounded up, so every plane starts on a 64-byte boundary
    u32 plane_stride;
    // Holds distances in params.div_size units, as u8 when narrow, with octant k of cell i at k*plane_stride + i.
    // A plane whose bit in built_octants is clear holds stale data.
    Array<u16> distances;
    // Holds the previous row of pixel distances, or for a non-octilinear octant, rows of them and of pass masks.
    // Each row starts on a 64-byte boundary, row_size() after the one before, with spare cells either side.
    Array<u16> row;
    u32 built_octants = 0;

    CoverGrid(chunk& c, Params params);
    ~CoverGrid() noexcept = default;

    void reset_for_reuse(chunk& ch, Params new_params);
    bool needs_rebuild() const { return is_marked_stale(); }
    void apply_params();
    // Returns true when no chunk at Chebyshev distance 2 changed after the build. Rays read RTrees that far.
    // check_current() doesn't call this, so the accessors' Debug check skips those chunks.
    bool is_ring2_current() const;
    /// @copydoc floormat::Grid::Cover::Grid::is_stale()
    bool is_stale() const;
    /// @copydoc floormat::Grid::Cover::Grid::is_stale(u64)
    bool is_stale(u64 pass_gen) const;
    template<typename T> T* octant_plane(u32 k);

    static u32 get_cell_index(u32 x, u32 y, u32 div_count);
    u32 get_cell_index_from_coord(intra_coord pos) const;

    void build_impl(chunk* self);
    bool fill_octant(u32 k, chunk& self);
    template<typename T> void fill_octilinear_octant(u32 k, chunk& self, const usize* bits, Vector2 dir,
                                                     i32 sx, i32 sy, float step_px);
    template<typename T> void fill_non_octilinear_octant(u32 k, chunk& self, const usize* bits);
    bool fill_next_unfilled_octant(chunk& self);
};

struct CoverPool : Pool<CoverGrid>
{
    Pass::Pool& pass_pool;
    u32 fills = 0;
    Ns fill_time{};

    CoverPool(Params p, Pass::Pool& pass_pool);
};

template<typename T>
T* CoverGrid::octant_plane(u32 k)
{
    fm_debug_assert(narrow == (sizeof(T) == 1));
    return reinterpret_cast<T*>(distances.data()) + k*plane_stride;
}

} // namespace floormat::Grid::detail
#endif

HWY_BEFORE_NAMESPACE();
namespace floormat::Grid::detail::HWY_NAMESPACE {
namespace {

using DU16 = hwy::HWY_NAMESPACE::CappedTag<u16, 16>;

template<typename D>
auto pass_mask(D d, const usize* pass_bits, u32 n)
{
    namespace hn = hwy::HWY_NAMESPACE;
    fm_debug3_assert(n % hn::Lanes(d) == 0);
    if constexpr (std::endian::native == std::endian::little)
        return hn::LoadMaskBits(d, reinterpret_cast<const u8*>(pass_bits) + n / 8);
    else if constexpr (hn::MaxLanes(d) <= 8)
        return hn::Dup128MaskFromMaskBits(d, (unsigned)(pass_bits[n / (sizeof(usize)*8)] >> n % (sizeof(usize)*8)));
    else
    {
        static_assert(hn::MaxLanes(d) <= sizeof(usize)*8, "a vector's cells must sit in one word");
        const auto bytes = std::byteswap((u64)(pass_bits[n / (sizeof(usize)*8)] >> n % (sizeof(usize)*8)));
        return hn::LoadMaskBits(d, reinterpret_cast<const u8*>(&bytes));
    }
}

// Computes an octilinear octant's row into prev and out, or returns false and writes nothing when the row doesn't
// split into whole vectors. It reads prev[-1] or prev[dc], which must be readable, and callers redo that cell.
template<u32 DivSize, typename T>
bool compute_row_vectorized(u16* __restrict prev, T* __restrict out, const usize* __restrict pass_bits,
                            u32 y, i32 sx, u32 step, u32 runtime_div_size)
{
    const u32 div_size = DivSize ? DivSize : runtime_div_size, dc = chunk_size_xy / div_size;
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr DU16 d;
    constexpr hn::Rebind<u8, decltype(d)> d8;
    const auto N = (u32)hn::Lanes(d);
    // Every vector must start on a byte of the pass grid, so the lane count must be a multiple of 8 and divide dc.
    if (N % 8 != 0 || dc % N != 0)
        return false;

    // Rows are dc cells apart, and dc is a multiple of N, so of 8.
    constexpr u32 row_bytes = (DivSize ? chunk_size_xy / DivSize : 8) * sizeof *out;
    constexpr u32 out_align = row_bytes < 64 ? row_bytes : 64; // xxx todo assumes power of two chunk size
    prev = assume_aligned<64>(prev);
    out = assume_aligned<out_align>(out);

    const auto vstep = hn::Set(d, (u16)step);
    const auto vone = hn::Set(d, u16{1});
    const auto vblocked = hn::Set(d, (u16)(T)-1);
    // px/unit rounded as (px/(unit/2) + 1)/2, since px + unit/2 can overflow
    const auto half_shift = std::countr_zero(div_size * fixed_one) - 1; // xxx todo assumes power of two chunk size
    const auto dx = sx < 0 ? 0u - N : N;
    for (u32 b = 0, x = sx < 0 ? dc - N : 0; b < dc; b += N, x += dx)
    {
        const auto next = hn::SaturatedAdd(hn::LoadU(d, prev + x + sx), vstep);
        const auto passable = pass_mask(d, pass_bits, y*dc + x);
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

template<typename T>
bool compute_row(u16* prev, T* out, const usize* pass_bits, u32 y, i32 sx, u32 step,
                 u32 div_size)
{
    static_assert(chunk_size_xy / 4 >= UINT8_MAX && chunk_size_xy / 8 < UINT8_MAX,
                  "u8 grids start at div_size 8");
    if constexpr (sizeof(T) == 2)
        switch (div_size)
        {
        case 1:  return compute_row_vectorized<1, T>(prev, out, pass_bits, y, sx, step, div_size);
        case 2:  return compute_row_vectorized<2, T>(prev, out, pass_bits, y, sx, step, div_size);
        case 4:  return compute_row_vectorized<4, T>(prev, out, pass_bits, y, sx, step, div_size);
        default: return compute_row_vectorized<0, T>(prev, out, pass_bits, y, sx, step, div_size);
        }
    else
        switch (div_size)
        {
        case 8:  return compute_row_vectorized<8, T>(prev, out, pass_bits, y, sx, step, div_size);
        case 16: return compute_row_vectorized<16, T>(prev, out, pass_bits, y, sx, step, div_size);
        case 32: return compute_row_vectorized<32, T>(prev, out, pass_bits, y, sx, step, div_size);
        case 64: return compute_row_vectorized<64, T>(prev, out, pass_bits, y, sx, step, div_size);
        default: return compute_row_vectorized<0, T>(prev, out, pass_bits, y, sx, step, div_size);
        }
}

// Returns true when the lane count is a multiple of 8 and divides dc, so every vector starts on a byte of
// the pass grid.
bool can_vectorize_non_octilinear_rows(u32 dc)
{
    namespace hn = hwy::HWY_NAMESPACE;
    const auto N = (u32)hn::Lanes(DU16{});
    return N % 8 == 0 && dc % N == 0;
}

// One u16 per cell, all ones where passable, so a vector can load from any cell.
void expand_pass_row(u16* __restrict out, const usize* __restrict pass_bits, u32 y, u32 dc)
{
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr DU16 d;
    const auto N = (u32)hn::Lanes(d);
    for (u32 x = 0; x < dc; x += N)
        hn::StoreU(hn::VecFromMask(d, pass_mask(d, pass_bits, y*dc + x)), d, out + x);
}

// Computes one row of octant o. next holds the distances |p| rows back, and masks[j] the pass masks j rows back.
// The |q| cells on the side q points to read spare cells of next and masks, so the caller redoes them.
template<typename T>
void compute_non_octilinear_row(const u16* __restrict next, u16* __restrict cur,
                                const u16* const* __restrict masks, T* __restrict out,
                                const non_octilinear_octant& o, u32 step, const u16* __restrict stop_px,
                                u32 div_size)
{
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr DU16 d;
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
        // o.cells runs farthest first, so px keeps the stop_px of the nearest blocked cell.
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

// An octilinear octant steps a whole cell at a time, so a passable cell is one step further than the
// next one along the octant. Only a cell whose next one is off the grid gets its distance from raycast_one().
template<typename T>
void fill_octilinear_octant(CoverGrid& g, u32 k, chunk& self, const usize* bits, Vector2 dir,
                            i32 sx, i32 sy, float step_px)
{
    const u32 div_size = g.params.div_size, dc = chunk_size_xy / div_size;
    auto& pass_pool = static_cast<CoverPool*>(g.pool)->pass_pool;
    T* const plane = g.octant_plane<T>(k);

    const Vector2i delta{Vector2(dir * (float)chunk_size_xy)};
    const auto ray_fixed = [&](u32 x, u32 y) {
        return to_fixed(raycast_one(self, x, y, div_size, delta, pass_pool));
    };
    // step_at(n) is the length of n offsets minus that of n - 1, each rounded, so a distance summed over any
    // number of consecutive offsets is off by at most 1/fixed_one px.
    const auto step_at = [step = (double)step_px * fixed_one](u32 n) {
        return (u32)(n*step + .5) - (u32)((n - 1)*step + .5);
    };

    if (sy == 0)
    {
        u16 steps[chunk_size_xy];
        for (u32 n = 1; n < dc; n++)
            steps[n] = (u16)step_at(n);
        // Each cell's distance is the next cell's along the row plus one offset's length, so rows are independent.
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

    // Each row needs the row computed before it. Looping over x in the direction of sx reads prev[x + sx]
    // before this row overwrites it.
    u16* const prev = g.row.data() + line_cells;
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
        if (n > 0 && compute_row(prev, plane + y*dc, bits, y, sx, step_at(n), div_size))
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

// Rows go from the edge p points at. A passable cell whose cell (q, p) away is off the grid gets its distance from
// raycast_one(). Others are one offset farther than that cell, or as far as the nearest blocked cell between them.
template<typename T>
void fill_non_octilinear_octant(CoverGrid& g, u32 k, chunk& self, const usize* bits)
{
    const u32 div_size = g.params.div_size, dc = chunk_size_xy / div_size;
    auto& pass_pool = static_cast<CoverPool*>(g.pool)->pass_pool;
    T* const plane = g.octant_plane<T>(k);
    const auto& o = non_octilinear_octants[k];
    const i32 q = o.q, p = o.p;
    const u32 aq = (u32)Math::abs(q), ap = (u32)Math::abs(p);
    const double len = Math::sqrt((double)(q*q + p*p));

    // delta is a multiple of (q, p), so the ray lies on the line through the centers of the cells whose distances
    // follow from its result.
    const auto delta = Vector2i{q, p} * (i32)Math::ceil(chunk_size_xy / len);
    const auto ray_fixed = [&](u32 x, u32 y) {
        return to_fixed(raycast_one(self, x, y, div_size, delta, pass_pool));
    };
    // R(n) is the length of n/|p| offsets, rounded, and row n from the edge p points at adds R(n) - R(n - |p|),
    // so a distance summed over any number of consecutive offsets is off by at most 1/fixed_one px.
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
    u16* const carries = g.row.data() + line_cells;
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

    const bool vectorize = can_vectorize_non_octilinear_rows(dc) && aq < dc;
    std::array<const u16*, ring_rows> row_masks;
    for (u32 n = 0; n < dc; n++)
    {
        const auto y = p > 0 ? dc - 1 - n : n;
        const auto step = n < ap ? 0 : Math::min(R(n) - R(n - ap), (u32)UINT16_MAX);
        if (vectorize)
            expand_pass_row(mask_row(n), bits, y, dc);
        if (vectorize && n >= ap)
        {
            for (u32 j = 0; j <= ap; j++)
                row_masks[j] = mask_row(n - j);
            compute_non_octilinear_row(carry_row(n - ap), carry_row(n), row_masks.data(), plane + y*dc, o,
                                       step, stop_px.data(), div_size);
            for (u32 m = 0; m < aq; m++)
                scalar_cell(n, q > 0 ? dc - 1 - m : m, y, step);
            continue;
        }
        for (u32 x = 0; x < dc; x++)
            scalar_cell(n, x, y, step);
    }
}

} // namespace
} // namespace floormat::Grid::detail::HWY_NAMESPACE
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace floormat::Grid::detail {

HWY_EXPORT(can_vectorize_non_octilinear_rows);
HWY_EXPORT(expand_pass_row);

bool can_vectorize_non_octilinear_rows(u32 dc)
{
    return HWY_DYNAMIC_DISPATCH(can_vectorize_non_octilinear_rows)(dc);
}

void expand_pass_row(u16* out, const usize* pass_bits, u32 y, u32 dc)
{
    HWY_DYNAMIC_DISPATCH(expand_pass_row)(out, pass_bits, y, dc);
}

CoverPool::CoverPool(Params p, Pass::Pool& pass_pool):
    Pool<CoverGrid>{p},
    pass_pool{pass_pool}
{
    fm_assert(pass_pool.params() == Pass::Params{params.div_size, pass_mode::shoot_through});
    reads_ring2 = true;
}

template<typename T>
void CoverGrid::fill_octilinear_octant(u32 k, chunk& self, const usize* bits, Vector2 dir,
                                       i32 sx, i32 sy, float step_px)
{
    HWY_EXPORT_AND_DYNAMIC_DISPATCH_T(fill_octilinear_octant<T>)(*this, k, self, bits, dir, sx, sy, step_px);
}

template<typename T>
void CoverGrid::fill_non_octilinear_octant(u32 k, chunk& self, const usize* bits)
{
    HWY_EXPORT_AND_DYNAMIC_DISPATCH_T(fill_non_octilinear_octant<T>)(*this, k, self, bits);
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
    // Without this, a rebuild leaves all octants marked built, holding the previous build's distances.
    built_octants = 0;
    fill_octant(0, *self);
}

bool CoverGrid::is_ring2_current() const
{
    const auto& w = c->world();
    for (auto off : ring2_offsets)
        if (const auto* ch = w.at(coord + off); ch && ch->pass_gen() > built_pass_gen)
            return false;
    return true;
}

bool CoverGrid::is_stale() const
{
    return is_marked_stale() || !is_current() || !is_ring2_current();
}

bool CoverGrid::is_stale(u64 pass_gen) const
{
    return pass_gen != built_pass_gen || !is_current() || !is_ring2_current();
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
    auto t = Time::now();

    const u32 dc = chunk_size_xy / div_size;

    auto pass_grid = pass_pool[self];
    pass_grid.build_if_stale();
    // pass bit and cell (x, y) are both y*dc + x, as pack_bit_index() lays them out
    fm_debug_assert(get_cell_index(1, 1, dc) == dc + 1);
    const usize* const bits = pass_grid.bits().data;

    const auto s = octant_steps[k];
    if (is_octilinear(s))
    {
        const auto dir = direction_for_octant(k);
        const i32 sx = s.x() > 0 ? 1 : s.x() < 0 ? -1 : 0;
        const i32 sy = s.y() > 0 ? 1 : s.y() < 0 ? -1 : 0;
        const auto step_px = (float)div_size / (sx ? Math::abs(dir.x()) : Math::abs(dir.y()));
        if (narrow)
            fill_octilinear_octant<u8>(k, self, bits, dir, sx, sy, step_px);
        else
            fill_octilinear_octant<u16>(k, self, bits, dir, sx, sy, step_px);
    }
    else if (narrow)
        fill_non_octilinear_octant<u8>(k, self, bits);
    else
        fill_non_octilinear_octant<u16>(k, self, bits);

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

bool CoverGrid::fill_next_unfilled_octant(chunk& self)
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

template struct Pool<CoverGrid>;

} // namespace floormat::Grid::detail

namespace floormat::Grid::Cover {

Params Params::validate() const
{
    fm_assert(div_size > 0);
    fm_assert(chunk_size_xy % div_size == 0);
    // A cover grid reads a pass grid of the same div_size, which Pass::Params::validate() requires to be below
    // chunk_size_xy.
    fm_assert(div_size < chunk_size_xy);
    fm_assert(div_size <= max_div_size);
    return *this;
}

bool Params::operator==(const Params&) const noexcept = default;

Vector2 octant_direction(u32 k)
{
    fm_debug_assert(k < octant_count);
    return detail::direction_for_octant(k);
}
Grid::~Grid() noexcept = default;
Grid::Grid(const Grid&) noexcept = default;
Grid& Grid::operator=(const Grid&) & noexcept = default;

Grid::Grid(detail::CoverGrid* grid_): grid{grid_} {}

u32 Grid::get_cell_index(u32 x, u32 y, u32 div_count)
{
    return detail::CoverGrid::get_cell_index(x, y, div_count);
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
u64 Grid::built_pass_gen() const { return grid->built_pass_gen; }

Grid::operator bool() const noexcept { return grid != nullptr; }
detail::CoverGrid* Grid::raw() const noexcept { return grid; }

bool Grid::is_marked_stale() const { return grid->is_marked_stale(); }
void Grid::mark_stale() { grid->mark_stale(); }
bool Grid::is_stale() const { return grid->is_stale(); }
bool Grid::is_stale(u64 pass_gen) const { return grid->is_stale(pass_gen); }

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

bool Grid::fill_next_unfilled_octant()
{
    grid->build_if_stale();
    return grid->fill_next_unfilled_octant(*grid->c);
}

u32 Grid::built_octants() const
{
    detail::check_current(grid);
    return grid->built_octants;
}

Grid Pool::operator[](chunk& c)
{
    return Grid{detail::pool_subscript(pool, c)};
}

void Pool::build_if_stale_all()
{
    for (auto* g : pool->grids_in_use)
        static_cast<detail::CoverGrid*>(g)->build_if_stale();
}

Pool::Pool(Params params, Pass::Pool& pass_pool): pool{new detail::CoverPool{params, pass_pool}} { }
Pool::~Pool() noexcept { delete pool; }
Params Pool::params() const { return pool->params; }
u32 Pool::free_grid_count() const { return pool->freelist.size(); }
u32 Pool::octant_fills() const { return pool->fills; }
Ns Pool::octant_fill_time() const { return pool->fill_time; }

} // namespace floormat::Grid::Cover
#endif
