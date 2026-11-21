#include "grid-pass.hpp"
#include "grid.inl"
#include "intra-coord.hpp"
#include "world.hpp"
#include "collision.hpp"
#include "object.hpp"
#include "search-pred.hpp"
#include "RTree-search.hpp"
#include "compat/debug.hpp"
#include "compat/function-view.hpp"
#include <bit>
#include <array>
#include <mg/Functions.h>

//#define FLOORMAT_GRID_PASS_DEBUG 1

namespace floormat {
} // namespace floormat

namespace floormat::Grid::detail {
namespace {
// The clear_rect_*() functions clear columns [i_lo, i_hi] of rows [j_lo, j_hi], where bit
// (i, j) is j*div_count + i. build_impl() picks one per build from div_count alone.

// Every row's span sits inside one word: rows pack whole into words, or rows are whole words
// and the span doesn't cross into the next one.
void clear_rect_in_word(usize* bits, u32 div_count, u32 i_lo, u32 i_hi, u32 j_lo, u32 j_hi)
{
    fm_debug3_assert(sizeof(usize)*8 % div_count == 0 ||
                     div_count % (sizeof(usize)*8) == 0 && i_lo / (sizeof(usize)*8) == i_hi / (sizeof(usize)*8));
    const usize span = ~usize{0} >> (sizeof(usize)*8 - 1 - (i_hi - i_lo));
    for (auto bit = j_lo * div_count + i_lo, end = j_hi * div_count + i_lo; bit <= end; bit += div_count)
        bits[bit / (sizeof(usize)*8)] &= ~(span << bit % (sizeof(usize)*8));
}

// Rows are whole words. Whether the span crosses a word boundary depends on the obstacle,
// so it's tested once here rather than per row.
void clear_rect_whole_rows(usize* bits, u32 div_count, u32 i_lo, u32 i_hi, u32 j_lo, u32 j_hi)
{
    fm_debug3_assert(div_count % (sizeof(usize)*8) == 0);
    const auto w0 = i_lo / (u32)(sizeof(usize)*8), w1 = i_hi / (u32)(sizeof(usize)*8);
    if (w0 == w1)
        return clear_rect_in_word(bits, div_count, i_lo, i_hi, j_lo, j_hi);

    const usize m_lo = ~usize{0} << i_lo % (sizeof(usize)*8);
    const usize m_hi = ~usize{0} >> (sizeof(usize)*8 - 1 - i_hi % (sizeof(usize)*8));
    const auto words_per_row = div_count / (u32)(sizeof(usize)*8);
    for (auto row = j_lo * words_per_row, end = j_hi * words_per_row; row <= end; row += words_per_row)
    {
        bits[row + w0] &= ~m_lo;
        for (auto k = w0 + 1; k < w1; k++)
            bits[row + k] = 0;
        bits[row + w1] &= ~m_hi;
    }
}

// LoadMaskBits() may read 8 bytes from a row's start, which runs past the grid on its last row.
u32 allocated_word_count(u32 div_count)
{
    return (bitmask_size(div_count) + 64) / (u32)(sizeof(usize)*8);
}

#ifdef FLOORMAT_GRID_PASS_DEBUG
unsigned grid_total_count = 0;
unsigned grid_alive_count = 0;
unsigned grid_free_count = 0;
unsigned pool_total_count = 0;
unsigned pool_alive_count = 0;
#endif
} // namespace

inflated_cells::inflated_cells(const Pass::Params& params):
    div_count{(i32)params.div_count}
{
    const auto div_size = params.div_size(), bbox_size = params.bbox_size;
    inv_div = 1.f / (float)div_size; // xxx todo assumes power of two chunk size
    // a box spans [center - bbox_lo, center + bbox_hi), as scenery_tile()
    const auto bbox_lo = (float)(bbox_size / 2), bbox_hi = (float)bbox_size - bbox_lo;
    if (params.min_pass == pass_mode::pass)
    {
        half_div_minus_half_tile = (float)params.node_offset() - half_tile<float>;
        grow_lo = bbox_hi;
        grow_hi = bbox_lo;
    }
    else
    {
        const auto half_div = (float)(div_size / 2);
        half_div_minus_half_tile = half_div - half_tile<float>;
        // A bit must be correct at every integer position in the cell. pack_bit_index_from_coord() floors, so the
        // cell spans [a - reach_below, a + reach_above] for a = i*div_size + half_div - half_tile.
        const auto reach_below = half_div;
        const auto reach_above = (float)(div_size - 1 - div_size / 2);
        // Intervals overlap when each one's low endpoint lies below the other's high endpoint.
        grow_lo = bbox_hi + reach_above;
        grow_hi = bbox_lo + reach_below;
    }
}

cell_range inflated_cells::operator()(float x0, float y0, float x1, float y1) const
{
    const float bx0 = x0 - grow_lo, by0 = y0 - grow_lo, bx1 = x1 + grow_hi, by1 = y1 + grow_hi;
    // rect_intersects() is strict, so cell i is included when bx0 < i*div_size + half_div_minus_half_tile < bx1.
    int i_lo = (int)Math::floor((bx0 - half_div_minus_half_tile) * inv_div) + 1;
    int i_hi = (int)Math::ceil ((bx1 - half_div_minus_half_tile) * inv_div) - 1;
    int j_lo = (int)Math::floor((by0 - half_div_minus_half_tile) * inv_div) + 1;
    int j_hi = (int)Math::ceil ((by1 - half_div_minus_half_tile) * inv_div) - 1;

    if (i_lo < 0) i_lo = 0;
    if (j_lo < 0) j_lo = 0;
    if (i_hi >= div_count) i_hi = div_count - 1;
    if (j_hi >= div_count) j_hi = div_count - 1;
    return { i_lo, i_hi, j_lo, j_hi };
}

using floormat::Grid::Pass::Params;

struct PassGrid : GridBase
{
    using Params = Grid::Pass::Params;

    Params params;
    Array<usize> bitmask;
    bool all_empty = false;

    PassGrid(chunk& c, Params params);
    ~PassGrid() noexcept;

    void reset_for_reuse(chunk& ch, Params new_params);
    bool needs_rebuild() const { return is_marked_stale(); }

    static u32 get_bitmask_index(u32 x, u32 y, u32 div_count);
    u32 get_bitmask_index_from_coord(intra_coord pos) const;
    bool is_all_empty() const noexcept { return all_empty; }

    void build_impl(chunk* self);
};

u32 PassGrid::get_bitmask_index(u32 x, u32 y, u32 div_count)
{
    return GridBase::pack_bit_index(x, y, div_count);
}

u32 PassGrid::get_bitmask_index_from_coord(intra_coord pos) const
{
    return GridBase::pack_bit_index_from_coord(pos, params.div_count);
}

void PassGrid::build_impl(chunk* self)
{
    for (auto& w : bitmask)
        w = ~usize{0};
    usize* const bits = bitmask.data();

    const auto div_countʹ = params.div_count;
    fm_assert(div_countʹ*div_countʹ <= bitmask.size() * (sizeof(usize)*8));

    const auto bbox_lo = (float)(params.bbox_size / 2), bbox_hi = (float)params.bbox_size - bbox_lo;
    const inflated_cells cells_of{params};

    chunk* const chunks[9] = {
        self,
        neighbors[0], neighbors[1], neighbors[2], neighbors[3],
        neighbors[4], neighbors[5], neighbors[6], neighbors[7],
    };
    static constexpr auto nb_offsets = []() {
        std::array<Vector2, 9> a{};
        for (auto i = 0u; i < 8; i++)
            a[i+1] = Vector2(world::neighbor_offsets[i]) * chunk_size<float>;
        return a;
    }();
    const float pmin_self[2] = { -half_tile<float> - bbox_lo, -half_tile<float> - bbox_lo };
    const float pmax_self[2] = { chunk_size<float> - half_tile<float> + bbox_hi,
                                 chunk_size<float> - half_tile<float> + bbox_hi };
    const auto min_pass = (u64)params.min_pass;
    all_empty = true;

    // Instantiated once per clear_rect, so the choice below is made once per build.
    auto clear_obstacles = [&](const auto& clear_rect) {
        for (auto n = 0u; n < 9; n++)
        {
            auto* c = chunks[n];
            if (!c)
                continue;
            const auto off = nb_offsets[n];
            const float pmin[2] = { pmin_self[0] - off.x(), pmin_self[1] - off.y() };
            const float pmax[2] = { pmax_self[0] - off.x(), pmax_self[1] - off.y() };
            c->rtree()->Search(pmin, pmax, [&](object_id data, const auto& r) {
                const auto x = std::bit_cast<collision_data>(data);
                fm_debug3_assert(x.type != (u64)collision_type::none);
                if (x.pass >= min_pass)
                    return true;
                if (Search::is_dynamic_object(*c, x))
                    return true;
                all_empty = false;

                const auto cr = cells_of(r.m_min[0] + off.x(), r.m_min[1] + off.y(),
                                         r.m_max[0] + off.x(), r.m_max[1] + off.y());
                if (cr.is_empty())
                    return true;

                clear_rect((u32)cr.i_lo, (u32)cr.i_hi, (u32)cr.j_lo, (u32)cr.j_hi);
                return true;
            });
        }
    };

    // div_count is a power of two, so it divides a word's bit count or is a multiple of it.
    if (sizeof(usize)*8 % div_countʹ == 0) [[likely]] // xxx todo assumes power of two chunk size
        clear_obstacles([=](u32 i_lo, u32 i_hi, u32 j_lo, u32 j_hi) {
            clear_rect_in_word(bits, div_countʹ, i_lo, i_hi, j_lo, j_hi);
        });
    else
        clear_obstacles([=](u32 i_lo, u32 i_hi, u32 j_lo, u32 j_hi) {
            clear_rect_whole_rows(bits, div_countʹ, i_lo, i_hi, j_lo, j_hi);
        });
}

PassGrid::PassGrid(chunk& c, Params params):
    GridBase{c},
    params{params},
    bitmask{ValueInit, allocated_word_count(params.div_count)}
{
#ifdef FLOORMAT_GRID_PASS_DEBUG
    ++grid_total_count;
    ++grid_alive_count;
    DBG << cat("PassGrid ctor: total=", grid_total_count) << cat("alive=", grid_alive_count)
        << cat("free=", grid_free_count);
#endif
}

PassGrid::~PassGrid() noexcept
{
#ifdef FLOORMAT_GRID_PASS_DEBUG
    --grid_alive_count;
    DBG << cat("PassGrid dtor: alive=", grid_alive_count) << cat("free=", grid_free_count);
#endif
}

void PassGrid::reset_for_reuse(chunk& ch, Params new_params)
{
    reset_base_for_reuse(ch);
    params = new_params;
    const auto words = allocated_word_count(params.div_count);
    if (bitmask.size() < words) [[unlikely]]
        bitmask = Array<usize>{ValueInit, words};
    else
        for (auto& w : bitmask)
            w = 0;
}

template struct Pool<PassGrid>;

struct PassPool final : Pool<PassGrid>
{
    using Pool<PassGrid>::Pool;
};

namespace {

template <typename T>
bool grid_bit_at(T* grid, u32 index)
{
    auto [word, mask] = GridBase::word_and_mask(index);
    return grid->bitmask[word] & mask;
}

namespace Tests {

constexpr bool assert_calc_div_size()
{
    fm_assert(calc_div_size(1, 8) == 1);
    fm_assert(calc_div_size(8, 8) == 8);
    fm_assert(calc_div_size(64, 64) == 64);
    fm_assert(calc_div_size(64, 128) == 64);
    fm_assert(calc_div_size(0, chunk_size_xy) == chunk_size_xy / 2);
    fm_assert(calc_div_size(chunk_size_xy, chunk_size_xy) == chunk_size_xy / 2);

    fm_assert(calc_div_size(0, 64) == 64);
    fm_assert(calc_div_size(0, 100) == 64);
    fm_assert(calc_div_size(0, 8) == 8);

    fm_assert(calc_div_size(1024, 64) == 64);
    fm_assert(calc_div_size(128, 100) == 64);

    fm_assert(calc_div_size(100, 200) == 64);
    fm_assert(calc_div_size(3, 8) == 2);
    fm_assert(calc_div_size(7, 8) == 4);

    return true;
}

static_assert(assert_calc_div_size());

} // namespace Tests
} // namespace
} // namespace floormat::Grid::detail

namespace floormat::Grid::Pass {

Params Params::validate() const
{
    fm_assert(min_pass > pass_mode::blocked && min_pass < pass_mode::COUNT);
    fm_assert(div_count > 0);
    fm_assert(chunk_size_xy % div_count == 0);
    // Search::cache's one-chunk margin holds A*'s seeds only for cells smaller than a chunk
    fm_assert(div_size() < chunk_size_xy);
    fm_assert(bbox_size >= div_size());
    fm_assert(bbox_size <= chunk_size_xy);
    return *this;
}

bool Params::operator==(const Params&) const noexcept = default;

Grid Pool::operator[](chunk& c)
{
    return Grid{detail::pool_subscript(pool, c), pool};
}

Grid::~Grid() noexcept = default;

Grid::Grid(detail::PassGrid* grid_, detail::PassPool* pool_):
    grid{grid_}, pool{pool_}
{
}

u32 Grid::get_bitmask_index(u32 x, u32 y, u32 div_count)
{
    return detail::PassGrid::get_bitmask_index(x, y, div_count);
}

u32 Grid::get_bitmask_index_from_coord(intra_coord pos) const
{
    return grid->get_bitmask_index_from_coord(pos);
}

bool Grid::bit(u32 index) const
{
    return detail::grid_bit_at(grid, index);
}

BitView Grid::bits() const
{
    detail::check_current(grid);
    return { grid->bitmask.data() };
}

u32 Grid::div_count() const
{
    return grid->params.div_count;
}

u64 Grid::build_no() const
{
    detail::check_current(grid);
    return grid->build_no;
}

u64 Grid::built_pass_gen() const
{
    return grid->built_pass_gen;
}

bool Grid::is_marked_stale() const
{
    return grid->is_marked_stale();
}

bool Grid::is_stale() const
{
    return grid->is_marked_stale() || !grid->is_current();
}

bool Grid::is_stale(u64 pass_gen) const
{
    return pass_gen != grid->built_pass_gen || !grid->is_current();
}

bool Grid::is_all_empty() const
{
    detail::check_current(grid);
    return grid->is_all_empty();
}


void Grid::mark_stale()
{
    grid->mark_stale();
    detail::cascade_mark_neighbors_stale(grid->coord, [this](chunk_coords_ ch) -> detail::GridBase* {
        auto* c = grid->c->world().at(ch);
        return c ? pool->find(*c) : nullptr;
    });
}

detail::PassGrid* Grid::raw() const noexcept { return grid; }
void Grid::build_if_stale() { grid->build_if_stale(); }

void Pool::build_if_stale_all()
{
    for (auto* g : pool->grids_in_use)
        static_cast<detail::PassGrid*>(g)->build_if_stale();
}

Pool::Pool(Params params): pool{new detail::PassPool{params}} { }
Pool::~Pool() noexcept { delete pool; }
const Params& Pool::params() const { return pool->params; }
u32 Pool::free_grid_count() const { return pool->freelist.size(); }

} // namespace floormat::Grid::Pass
