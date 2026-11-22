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
#include <cstring>
#include <numeric>
#include <type_traits>
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
            fm_assert(major < (aq > ap ? aq : ap));
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

// Returns the distance in px from cell (cell_x, cell_y)'s center to the first collider toward the center plus
// delta, at most max_ray_px. It calls do_raycasting() directly, since raycast() reads the clock twice per ray.
[[maybe_unused]]
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

constexpr u32 line_cells = 64 / sizeof(u16);
// The coarsest grid's div_count(). A fill computes multiples of it per row, which split into whole vectors.
constexpr u32 min_div_count = chunk_size_xy / Grid::Cover::max_div_size;

u32 round_up(u32 n, u32 multiple)
{
    return (n + multiple - 1) / multiple * multiple;
}

u32 row_size(u32 dc)
{
    return 2*round_up(dc, line_cells) + 2*line_cells;
}

// Read as every row of a chunk that is missing or has no blocked cell. LoadMaskBits() may read 8 bytes from a
// cell's bit, so one more word follows the chunk_size_xy bits.
constexpr auto passable_row_bits = [] {
    std::array<usize, chunk_size_xy / (sizeof(usize)*8) + 1> ret;
    for (auto& word : ret)
        word = ~usize{0};
    return ret;
}();

// Returns how many cells of a row are passable from bit first_bit of pass_bits, the way sx points, up to the
// first blocked one, and at most max_cells.
u32 passable_run_length(const usize* pass_bits, u32 first_bit, i32 sx, u32 max_cells)
{
    constexpr u32 word_bits = sizeof(usize)*8;
    u32 n = 0;
    while (n < max_cells)
    {
        const u32 bit = sx > 0 ? first_bit + n : first_bit - n, shift = bit % word_bits;
        const usize word = pass_bits[bit / word_bits];
        // the zeros shifted in become ones, which end the count at the word's edge
        const u32 run_length = sx > 0 ? (u32)std::countr_zero((usize)~(word >> shift))
                                      : (u32)std::countl_zero((usize)~(word << (word_bits - 1 - shift)));
        n += run_length;
        if (run_length < (sx > 0 ? word_bits - shift : shift + 1))
            break;
    }
    return Math::min(n, max_cells);
}

} // namespace

// A chunk's pass bits: cell (x, y) is bit y*row_stride + x of data. A chunk that is missing or has no blocked
// cell gets passable_row_bits and a row_stride of 0.
struct chunk_pass_bits
{
    const usize* data;
    u32 row_stride;
};

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
    // Each row spans two chunks and starts on a 64-byte boundary, row_size() after the one before.
    Array<u16> row;
    u32 built_octants = 0;

    CoverGrid(chunk& c, Params params);
    ~CoverGrid() noexcept = default;

    void reset_for_reuse(chunk& ch, Params new_params);
    bool needs_rebuild() const { return is_marked_stale(); }
    void apply_params();
    // Returns true when no chunk at Chebyshev distance 2 changed after the build. The neighbors' pass grids read
    // RTrees that far. check_current() doesn't call this, so the accessors' Debug check skips those chunks.
    bool is_ring2_current() const;
    /// @copydoc floormat::Grid::Cover::Grid::is_stale()
    bool is_stale() const;
    /// @copydoc floormat::Grid::Cover::Grid::is_stale(u64)
    bool is_stale(u64 pass_gen) const;
    template<typename T> T* octant_plane(u32 k);

    static u32 get_cell_index(u32 x, u32 y, u32 div_count);
    u32 get_cell_index_from_coord(intra_coord pos) const;

    void build_impl(chunk* self);
    // Returns the chunk at offset from the grid's own, or null when there is none or offset leaves the world.
    chunk* neighbor_chunk_at(Vector2i offset) const;
    bool fill_octant(u32 k, chunk& self);
    // pass_bits[iy*2 + ix] is the chunk at (ix, iy) times the signs of the octant's cell offset.
    template<typename T> void fill_octilinear_octant(u32 k, const std::array<chunk_pass_bits, 4>& pass_bits,
                                                     u32 num_neighbor_columns, u32 num_neighbor_rows,
                                                     i32 sx, i32 sy, float step_px);
    template<typename T> void fill_non_octilinear_octant(u32 k, const std::array<chunk_pass_bits, 4>& pass_bits,
                                                         u32 num_neighbor_columns, u32 num_neighbor_rows);
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
static_assert(hwy::HWY_NAMESPACE::MaxLanes(DU16{}) <= min_div_count,
              "the coarsest grid's row must split into whole vectors"); // xxx todo assumes power of two chunk size

template<typename D>
auto pass_mask(D d, const usize* pass_bits, u32 n)
{
    namespace hn = hwy::HWY_NAMESPACE;
    fm_debug3_assert(n % hn::Lanes(d) == 0);
    if constexpr (std::endian::native == std::endian::little && hn::MaxLanes(d) >= 8)
    {
        fm_debug3_assert(hn::Lanes(d) % 8 == 0);
        return hn::LoadMaskBits(d, reinterpret_cast<const u8*>(pass_bits) + n / 8);
    }
    else if constexpr (hn::MaxLanes(d) <= 8)
        return hn::Dup128MaskFromMaskBits(d, (unsigned)(pass_bits[n / (sizeof(usize)*8)] >> n % (sizeof(usize)*8)));
    else
    {
        static_assert(hn::MaxLanes(d) <= sizeof(usize)*8, "a vector's cells must sit in one word");
        const auto bytes = std::byteswap((u64)(pass_bits[n / (sizeof(usize)*8)] >> n % (sizeof(usize)*8)));
        return hn::LoadMaskBits(d, reinterpret_cast<const u8*>(&bytes));
    }
}

// Stores the pixel distances px rounded to div_size units, or `blocked` where a cell isn't passable.
template<typename T, typename V, typename M>
void store_rounded_distances(T* out, V px, M passable, u32 div_size)
{
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr DU16 d;
    constexpr hn::Rebind<u8, decltype(d)> d8;
    // px/unit rounded as (px/(unit/2) + 1)/2, since px + unit/2 can overflow
    const auto half_shift = std::countr_zero(div_size * fixed_one) - 1; // xxx todo assumes power of two chunk size
    const auto rounded = hn::ShiftRight<1>(hn::Add(hn::ShiftRightSame(px, half_shift), hn::Set(d, u16{1})));
    const auto units = hn::IfThenElse(passable, rounded, hn::Set(d, (u16)(T)-1));
    if constexpr (sizeof(T) == 1)
        hn::StoreU(hn::DemoteTo(d8, units), d8, out);
    else
        hn::StoreU(units, d, out);
}

// Computes cell_count cells of an octilinear octant's row in place, from row[first_cell] and bit first_bit, and
// unless T is void, their rounded distances into out. A nonzero DivSize makes them a chunk's own row.
template<u32 DivSize, typename T>
void compute_row_vectorized(u16* __restrict row, T* __restrict out, const usize* __restrict pass_bits,
                            u32 first_bit, isize first_cell, u32 cell_count, i32 sx, u32 step,
                            [[maybe_unused]] u32 runtime_div_size)
{
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr DU16 d;
    const auto N = (u32)hn::Lanes(d);
    if constexpr (DivSize != 0)
    {
        fm_debug3_assert(first_cell == 0 && cell_count == chunk_size_xy / DivSize);
        first_cell = 0;
        cell_count = chunk_size_xy / DivSize;
    }
    fm_debug3_assert(cell_count % N == 0);
    row = assume_aligned<64>(row);
    if constexpr (!std::is_void_v<T>)
    {
        // Rows are dc cells apart, and dc is a multiple of 8.
        constexpr u32 row_bytes = (DivSize ? chunk_size_xy / DivSize : 8) * sizeof(T);
        constexpr u32 out_align = row_bytes < 64 ? row_bytes : 64; // xxx todo assumes power of two chunk size
        out = assume_aligned<out_align>(out);
    }

    const auto vstep = hn::Set(d, (u16)step);
    const auto dx = sx < 0 ? 0u - N : N;
    // Going the way sx points reads row[x + sx] before this row overwrites it.
    for (u32 b = 0, x = sx < 0 ? cell_count - N : 0; b < cell_count; b += N, x += dx)
    {
        u16* const cells = row + (first_cell + (isize)x);
        const auto next = hn::SaturatedAdd(hn::LoadU(d, cells + sx), vstep);
        const auto passable = pass_mask(d, pass_bits, first_bit + x);
        const auto px = hn::IfThenElseZero(passable, next);
        hn::StoreU(px, d, cells);
        if constexpr (!std::is_void_v<T>)
            store_rounded_distances(out + x, px, passable, DivSize ? DivSize : runtime_div_size);
    }
}

// Computes the chunk's own row of an octilinear octant in place, and its rounded distances into out.
template<typename T>
void compute_row(u16* row, T* out, const usize* pass_bits, u32 first_bit, i32 sx, u32 step, u32 div_size)
{
    static_assert(chunk_size_xy / 4 >= UINT8_MAX && chunk_size_xy / 8 < UINT8_MAX,
                  "u8 grids start at div_size 8");
    const u32 dc = chunk_size_xy / div_size;
    if constexpr (sizeof(T) == 2)
        switch (div_size)
        {
        case 1:  return compute_row_vectorized<1, T>(row, out, pass_bits, first_bit, 0, dc, sx, step, div_size);
        case 2:  return compute_row_vectorized<2, T>(row, out, pass_bits, first_bit, 0, dc, sx, step, div_size);
        case 4:  return compute_row_vectorized<4, T>(row, out, pass_bits, first_bit, 0, dc, sx, step, div_size);
        default: return compute_row_vectorized<0, T>(row, out, pass_bits, first_bit, 0, dc, sx, step, div_size);
        }
    else
        switch (div_size)
        {
        case 8:  return compute_row_vectorized<8, T>(row, out, pass_bits, first_bit, 0, dc, sx, step, div_size);
        case 16: return compute_row_vectorized<16, T>(row, out, pass_bits, first_bit, 0, dc, sx, step, div_size);
        case 32: return compute_row_vectorized<32, T>(row, out, pass_bits, first_bit, 0, dc, sx, step, div_size);
        case 64: return compute_row_vectorized<64, T>(row, out, pass_bits, first_bit, 0, dc, sx, step, div_size);
        default: return compute_row_vectorized<0, T>(row, out, pass_bits, first_bit, 0, dc, sx, step, div_size);
        }
}

// Stores how many cells lie from each cell of a row to the first blocked cell the way sx points, at most dc.
// num_passable_cells_along_sx counts from the cell that follows the row to the first blocked one.
template<typename T>
void compute_horizontal_octant_row(T* __restrict out, const usize* __restrict pass_bits, u32 first_bit, u32 dc,
                                   i32 sx, u32 num_passable_cells_along_sx)
{
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr DU16 d;
    constexpr hn::Rebind<u8, decltype(d)> d8;
    const auto N = (u32)hn::Lanes(d);
    fm_debug3_assert(dc % N == 0);

    // A position counts cells against sx. The cell that follows a vector's cells the way sx points is at dc, so
    // every position that matters is above 0, which stands for no blocked cell.
    const auto lane = hn::Iota(d, 0);
    const auto lane_positions = hn::Add(sx > 0 ? hn::Sub(hn::Set(d, (u16)(N - 1)), lane) : lane,
                                        hn::Set(d, (u16)(dc + 1)));
    const auto dx = sx > 0 ? 0u - N : N;
    for (u32 b = 0, x = sx > 0 ? dc - N : 0; b < dc; b += N, x += dx)
    {
        const auto passable = pass_mask(d, pass_bits, first_bit + x);
        auto nearest_blocked_position = hn::IfThenZeroElse(passable, lane_positions);
        // Each lane takes the maximum over the lanes following it the way sx points, twice as many per pass.
        for (u32 n = 1; n < N; n *= 2)
            nearest_blocked_position = hn::Max(nearest_blocked_position,
                                               sx > 0 ? hn::SlideDownLanes(d, nearest_blocked_position, n)
                                                      : hn::SlideUpLanes(d, nearest_blocked_position, n));
        nearest_blocked_position = hn::Max(nearest_blocked_position,
                                           hn::Set(d, (u16)(dc - Math::min(num_passable_cells_along_sx, dc - 1))));
        const auto cells = hn::Min(hn::Sub(lane_positions, nearest_blocked_position), hn::Set(d, (u16)dc));
        const auto units = hn::IfThenElse(passable, cells, hn::Set(d, (u16)(T)-1));
        if constexpr (sizeof(T) == 1)
            hn::StoreU(hn::DemoteTo(d8, units), d8, out + x);
        else
            hn::StoreU(units, d, out + x);

        const auto run_length = passable_run_length(pass_bits, first_bit + (sx > 0 ? x : x + N - 1), sx, N);
        num_passable_cells_along_sx = run_length < N ? run_length : num_passable_cells_along_sx + N;
    }
}

// Writes one u16 per cell, all ones where passable, so a vector can load from any cell. first_bit and
// cell_count must be multiples of the lane count.
void expand_pass_row(u16* __restrict out, const usize* __restrict pass_bits, u32 first_bit, u32 cell_count)
{
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr DU16 d;
    const auto N = (u32)hn::Lanes(d);
    for (u32 x = 0; x < cell_count; x += N)
        hn::StoreU(hn::VecFromMask(d, pass_mask(d, pass_bits, first_bit + x)), d, out + x);
}

// Computes cell_count cells of one row of octant o, from cell first_cell, into cur and, unless T is void, out.
// next holds the pixel distances |p| rows back, and crossed_cell_masks[c] the pass masks o.cells[c] away.
template<typename T>
void compute_non_octilinear_row(const u16* __restrict next, u16* __restrict cur, const u16* __restrict masks,
                                const u16* const* __restrict crossed_cell_masks, T* __restrict out,
                                const non_octilinear_octant& o, u32 step, const u16* __restrict stop_px,
                                isize first_cell, u32 cell_count, [[maybe_unused]] u32 div_size)
{
    namespace hn = hwy::HWY_NAMESPACE;
    constexpr DU16 d;
    const auto N = (u32)hn::Lanes(d);
    fm_debug3_assert(cell_count % N == 0);

    const auto vstep = hn::Set(d, (u16)step);
    for (u32 x = 0; x < cell_count; x += N)
    {
        const auto i = first_cell + (isize)x;
        auto px = hn::SaturatedAdd(hn::LoadU(d, next + (i + o.q)), vstep);
        // No stop_px exceeds step, so the minimum is the nearest blocked crossed cell's stop_px, if any.
        for (u32 c = 0; c < o.count; c++)
            px = hn::Min(px, hn::Or(hn::LoadU(d, crossed_cell_masks[c] + i), hn::Set(d, stop_px[c])));
        const auto passable = hn::MaskFromVec(hn::LoadU(d, masks + i));
        px = hn::IfThenElseZero(passable, px);
        hn::StoreU(px, d, cur + i);
        if constexpr (!std::is_void_v<T>)
            store_rounded_distances(out + x, px, passable, div_size);
    }
}

// A passable cell is one cell offset farther than the next cell along the octant. Rows before the first hold
// UINT16_MAX, which rounds to dc.
template<typename T>
void fill_octilinear_octant(CoverGrid& g, u32 k, const std::array<chunk_pass_bits, 4>& pass_bits,
                            u32 num_neighbor_columns, u32 num_neighbor_rows, i32 sx, i32 sy, float step_px)
{
    const u32 div_size = g.params.div_size, dc = chunk_size_xy / div_size;
    T* const plane = g.octant_plane<T>(k);

    if (sy == 0)
    {
        const usize* const own_bits = pass_bits[0].data;
        const auto& neighbor_bits = pass_bits[1];
        // The offset is exactly one cell, so the plane holds cell counts, and rows are independent.
        for (u32 y = 0; y < dc; y++)
        {
            u32 num_passable_cells_along_sx = dc;
            if (num_neighbor_columns)
            {
                const auto first_bit = y*neighbor_bits.row_stride + (sx > 0 ? 0 : dc - 1);
                const auto run_length =
                    passable_run_length(neighbor_bits.data, first_bit, sx, num_neighbor_columns);
                if (run_length < num_neighbor_columns)
                    num_passable_cells_along_sx = run_length;
            }
            compute_horizontal_octant_row(plane + y*dc, own_bits, y*dc, dc, sx, num_passable_cells_along_sx);
        }
    }
    else
    {
        // Row n adds S(n) - S(n - 1), the rounded lengths of n and n - 1 offsets, so a distance summed over any
        // number of consecutive offsets is off by at most 1/fixed_one px. n is negative in a neighbor's rows.
        const auto S = [step = (double)step_px * fixed_one](i32 n) { return (i32)Math::floor(n*step + .5); };

        u16* const slot = g.row.data();
        std::memset(slot, 0xff, row_size(dc) * sizeof *slot);
        u16* const row = slot + line_cells + (sx < 0 ? round_up(dc, line_cells) : 0);
        const auto neighbor_first_cell = sx > 0 ? (isize)dc : -(isize)num_neighbor_columns;
        const u32 neighbor_first_x = sx > 0 ? 0 : dc - num_neighbor_columns;

        for (i32 n = -(i32)num_neighbor_rows; n < (i32)dc; n++)
        {
            const u32 iy = n < 0;
            const auto y = (u32)((sy > 0 ? (i32)dc - 1 - n : n) + (i32)dc) % dc;
            const auto step = (u32)(S(n) - S(n - 1));
            const chunk_pass_bits &bits = pass_bits[iy*2], &neighbor_bits = pass_bits[iy*2 + 1];
            // The own columns' last vector reads a neighbor's cell from the row before, so they go first.
            if (n >= 0)
                compute_row(row, plane + y*dc, bits.data, y*bits.row_stride, sx, step, div_size);
            else
                compute_row_vectorized<0, void>(row, nullptr, bits.data, y*bits.row_stride, 0, dc, sx, step,
                                                div_size);
            if (num_neighbor_columns)
                compute_row_vectorized<0, void>(row, nullptr, neighbor_bits.data,
                                                y*neighbor_bits.row_stride + neighbor_first_x, neighbor_first_cell,
                                                num_neighbor_columns, sx, step, div_size);
        }
    }
}

// Rows go against p, starting num_neighbor_rows before the chunk's own. A passable cell is one offset farther
// than the cell (q, p) away, or as far as the nearest blocked cell between them.
template<typename T>
void fill_non_octilinear_octant(CoverGrid& g, u32 k, const std::array<chunk_pass_bits, 4>& pass_bits,
                                u32 num_neighbor_columns, u32 num_neighbor_rows)
{
    const u32 div_size = g.params.div_size, dc = chunk_size_xy / div_size;
    T* const plane = g.octant_plane<T>(k);
    const auto& o = non_octilinear_octants[k];
    const i32 q = o.q, p = o.p;
    const u32 aq = (u32)Math::abs(q), ap = (u32)Math::abs(p);
    const double len = Math::sqrt((double)(q*q + p*p));

    // R(n) is the length of n/|p| offsets, rounded, and row n adds R(n) - R(n - |p|), so a distance summed over
    // any number of consecutive offsets is off by at most 1/fixed_one px. n is negative in a neighbor's rows.
    const double row_fixed = div_size * len * fixed_one / ap;
    const auto R = [row_fixed](i32 n) { return (i32)Math::floor(n*row_fixed + .5); };
    std::array<u16, std::tuple_size_v<decltype(o.cells)>> stop_px;
    for (u32 c = 0; c < o.count; c++)
    {
        const double major_px = o.cells[c].major * div_size * len / Math::max(aq, ap);
        stop_px[c] = (u16)Math::min((u32)(major_px*fixed_one + .5), (u32)UINT16_MAX);
    }

    const u32 slots = ap + 1, rs = row_size(dc);
    fm_debug_assert(slots <= ring_rows);
    // Rows before the first read as passable and UINT16_MAX px away, as do cells past the neighbor's columns.
    std::memset(g.row.data(), 0xff, 2*ring_rows*rs * sizeof(u16));
    u16* const pixel_distances = g.row.data() + line_cells + (q < 0 ? round_up(dc, line_cells) : 0);
    u16* const masks = pixel_distances + ring_rows*rs;
    const auto pixel_distance_row = [&](u32 r) { return pixel_distances + r % slots * rs; };
    const auto mask_row = [&](u32 r) { return masks + r % slots * rs; };
    const auto neighbor_first_cell = q > 0 ? (isize)dc : -(isize)num_neighbor_columns;
    const u32 neighbor_first_x = q > 0 ? 0 : dc - num_neighbor_columns;

    std::array<const u16*, std::tuple_size_v<decltype(o.cells)>> crossed_cell_masks;
    for (i32 n = -(i32)num_neighbor_rows; n < (i32)dc; n++)
    {
        const auto r = (u32)(n + (i32)num_neighbor_rows);
        const u32 iy = n < 0;
        const auto y = (u32)((p > 0 ? (i32)dc - 1 - n : n) + (i32)dc) % dc;
        const auto step = (u32)Math::min(R(n) - R(n - (i32)ap), (i32)UINT16_MAX);
        const chunk_pass_bits &bits = pass_bits[iy*2], &neighbor_bits = pass_bits[iy*2 + 1];

        u16* const mask = mask_row(r);
        expand_pass_row(mask, bits.data, y*bits.row_stride, dc);
        if (num_neighbor_columns)
            expand_pass_row(mask + neighbor_first_cell, neighbor_bits.data,
                            y*neighbor_bits.row_stride + neighbor_first_x, num_neighbor_columns);
        for (u32 c = 0; c < o.count; c++)
        {
            const auto& cell = o.cells[c];
            crossed_cell_masks[c] = mask_row(r + slots - (u32)(cell.y < 0 ? -cell.y : cell.y)) + cell.x;
        }
        const u16* const next = pixel_distance_row(r + 1);
        u16* const cur = pixel_distance_row(r);
        if (n < 0)
            compute_non_octilinear_row<void>(next, cur, mask, crossed_cell_masks.data(), nullptr, o, step,
                                             stop_px.data(), q > 0 ? 0 : neighbor_first_cell,
                                             dc + num_neighbor_columns, div_size);
        else
        {
            compute_non_octilinear_row(next, cur, mask, crossed_cell_masks.data(), plane + y*dc, o, step,
                                       stop_px.data(), 0, dc, div_size);
            if (num_neighbor_columns)
                compute_non_octilinear_row<void>(next, cur, mask, crossed_cell_masks.data(), nullptr, o, step,
                                                 stop_px.data(), neighbor_first_cell, num_neighbor_columns,
                                                 div_size);
        }
    }
}

} // namespace
} // namespace floormat::Grid::detail::HWY_NAMESPACE
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace floormat::Grid::detail {

HWY_EXPORT(expand_pass_row);

void expand_pass_row(u16* out, const usize* pass_bits, u32 first_bit, u32 cell_count)
{
    HWY_DYNAMIC_DISPATCH(expand_pass_row)(out, pass_bits, first_bit, cell_count);
}

CoverPool::CoverPool(Params p, Pass::Pool& pass_pool):
    Pool<CoverGrid>{p},
    pass_pool{pass_pool}
{
    fm_assert(pass_pool.params() == Pass::Params{params.div_size, pass_mode::shoot_through});
    reads_ring2 = true;
}

template<typename T>
void CoverGrid::fill_octilinear_octant(u32 k, const std::array<chunk_pass_bits, 4>& pass_bits,
                                       u32 num_neighbor_columns, u32 num_neighbor_rows,
                                       i32 sx, i32 sy, float step_px)
{
    HWY_EXPORT_AND_DYNAMIC_DISPATCH_T(fill_octilinear_octant<T>)(*this, k, pass_bits, num_neighbor_columns,
                                                                 num_neighbor_rows, sx, sy, step_px);
}

template<typename T>
void CoverGrid::fill_non_octilinear_octant(u32 k, const std::array<chunk_pass_bits, 4>& pass_bits,
                                           u32 num_neighbor_columns, u32 num_neighbor_rows)
{
    HWY_EXPORT_AND_DYNAMIC_DISPATCH_T(fill_non_octilinear_octant<T>)(*this, k, pass_bits, num_neighbor_columns,
                                                                     num_neighbor_rows);
}

chunk* CoverGrid::neighbor_chunk_at(Vector2i offset) const
{
    const auto x = coord.x + offset.x(), y = coord.y + offset.y();
    if (x < chunk_xy_min || x > chunk_xy_max || y < chunk_xy_min || y > chunk_xy_max)
        return nullptr;
    else
        return c->world().at({(i16)x, (i16)y, coord.z});
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
    // pass bit and cell (x, y) are both y*dc + x, as pack_bit_index() lays them out
    fm_debug_assert(get_cell_index(1, 1, dc) == dc + 1);

    const auto s = octant_steps[k];
    const i32 sx = s.x() > 0 ? 1 : s.x() < 0 ? -1 : 0;
    const i32 sy = s.y() > 0 ? 1 : s.y() < 0 ? -1 : 0;
    const std::array<chunk*, 4> chunks = {
        &self,
        sx ? neighbor_chunk_at({sx, 0}) : nullptr,
        sy ? neighbor_chunk_at({0, sy}) : nullptr,
        sx && sy ? neighbor_chunk_at({sx, sy}) : nullptr,
    };
    for (auto* ch : chunks)
        if (ch)
            pass_pool[*ch].build_if_stale();

    std::array<chunk_pass_bits, 4> pass_bits;
    for (u32 i = 0; i < chunks.size(); i++)
    {
        pass_bits[i] = {passable_row_bits.data(), 0};
        if (!chunks[i])
            continue;
        const auto pass_grid = pass_pool[*chunks[i]];
        if (i == 0 || !pass_grid.is_all_empty())
            pass_bits[i] = {pass_grid.bits().data, dc};
    }
    const u32 aq = (u32)Math::abs(s.x()), ap = (u32)Math::abs(s.y()), major = Math::max(aq, ap);
    // A blocked cell lowers a distance in the chunk below dc only when it lies at most this many cells from the
    // chunk along the axis the offset is longer on.
    const auto max_cells_along_major_axis = (u32)(dc*major / Math::sqrt((double)(aq*aq + ap*ap)));
    // Lines that long, and the cells they cross, leave the chunk by at most this many columns and rows.
    const auto max_neighbor_columns_and_rows =
        Math::min(Vector2ui{max_cells_along_major_axis},
                  ((2*max_cells_along_major_axis + 1)*Vector2ui{aq, ap} + Vector2ui{major}) / (2*major));
    // Cells of chunks with no blocked cell would all hold UINT16_MAX, as cells left out do.
    const u32 num_neighbor_columns = pass_bits[1].row_stride || pass_bits[3].row_stride
                                     ? Math::min(round_up(max_neighbor_columns_and_rows.x(), min_div_count), dc) : 0;
    const u32 num_neighbor_rows = pass_bits[2].row_stride || pass_bits[3].row_stride
                                  ? max_neighbor_columns_and_rows.y() : 0;

    if (is_octilinear(s))
    {
        const auto dir = direction_for_octant(k);
        const auto step_px = (float)div_size / (sx ? Math::abs(dir.x()) : Math::abs(dir.y()));
        if (narrow)
            fill_octilinear_octant<u8>(k, pass_bits, num_neighbor_columns, num_neighbor_rows, sx, sy, step_px);
        else
            fill_octilinear_octant<u16>(k, pass_bits, num_neighbor_columns, num_neighbor_rows, sx, sy, step_px);
    }
    else if (narrow)
        fill_non_octilinear_octant<u8>(k, pass_bits, num_neighbor_columns, num_neighbor_rows);
    else
        fill_non_octilinear_octant<u16>(k, pass_bits, num_neighbor_columns, num_neighbor_rows);

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

u32 Grid::ensure_octant(u32 k)
{
    fm_debug_assert(k < octant_count);
    const auto& P = *static_cast<const detail::CoverPool*>(grid->pool);
    const auto fills_at_start = P.fills;
    grid->build_if_stale();
    (void)grid->fill_octant(k, *grid->c);
    return P.fills - fills_at_start;
}

u32 Grid::fill_next_unfilled_octant()
{
    const auto& P = *static_cast<const detail::CoverPool*>(grid->pool);
    const auto fills_at_start = P.fills;
    grid->build_if_stale();
    (void)grid->fill_next_unfilled_octant(*grid->c);
    return P.fills - fills_at_start;
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
