#include "grid-pass.hpp"
#include "grid.inl"
#include "intra-coord.hpp"
#include "world.hpp"
#include "collision.hpp"
#include "object.hpp"
#include "search.hpp"
#include "src/RTree-search.hpp"
#include "compat/array-size.hpp"
#include "compat/function2.hpp"
#include <bit>
#include <array>
#include <cstring>
#include <cr/BitArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>

//#define FLOORMAT_GRID_PASS_DEBUG 1

namespace floormat {
} // namespace floormat

namespace floormat::detail::grid {
namespace {
// Bit n of the grid is bit (n & 7) of byte (n >> 3), as BitArray stores it. Read as
// little-endian 64-bit words, that's bit (n & 63) of word (n >> 6) on any host.
uint64_t load_word(const uint8_t* bits, uint32_t word)
{
    uint64_t w;
    std::memcpy(&w, bits + word * sizeof w, sizeof w);
    if constexpr (std::endian::native == std::endian::big)
        w = std::byteswap(w);
    return w;
}

void store_word(uint8_t* bits, uint32_t word, uint64_t w)
{
    if constexpr (std::endian::native == std::endian::big)
        w = std::byteswap(w);
    std::memcpy(bits + word * sizeof w, &w, sizeof w);
}

void clear_word_bits(uint8_t* bits, uint32_t word, uint64_t mask)
{
    store_word(bits, word, load_word(bits, word) & ~mask);
}

// The clear_rect_*() functions clear columns [i_lo, i_hi] of rows [j_lo, j_hi], where bit
// (i, j) is j*div_count + i. build_impl() picks one per build from div_count alone.

// Every row's span sits inside one word: rows pack whole into words (64 % div_count == 0),
// or rows are whole words and the span doesn't cross a multiple of 64.
void clear_rect_in_word(uint8_t* bits, uint32_t div_count, uint32_t i_lo, uint32_t i_hi, uint32_t j_lo, uint32_t j_hi)
{
    fm_debug3_assert(64 % div_count == 0 || (div_count % 64 == 0 && i_lo >> 6 == i_hi >> 6));
    const uint64_t span = ~uint64_t{0} >> (63 - (i_hi - i_lo));
    for (auto bit = j_lo * div_count + i_lo, end = j_hi * div_count + i_lo; bit <= end; bit += div_count)
        clear_word_bits(bits, bit >> 6, span << (bit & 63));
}

// Rows are whole words (div_count % 64 == 0). Whether the span crosses a word boundary
// depends on the obstacle, so it's tested once here rather than per row.
void clear_rect_whole_rows(uint8_t* bits, uint32_t div_count, uint32_t i_lo, uint32_t i_hi, uint32_t j_lo, uint32_t j_hi)
{
    fm_debug3_assert(div_count % 64 == 0);
    const auto w0 = i_lo >> 6, w1 = i_hi >> 6;
    if (w0 == w1)
        return clear_rect_in_word(bits, div_count, i_lo, i_hi, j_lo, j_hi);

    const uint64_t m_lo = ~uint64_t{0} << (i_lo & 63);
    const uint64_t m_hi = ~uint64_t{0} >> (63 - (i_hi & 63));
    const auto words_per_row = div_count >> 6;
    for (auto row = j_lo * words_per_row, end = j_hi * words_per_row; row <= end; row += words_per_row)
    {
        clear_word_bits(bits, row + w0, m_lo);
        for (auto k = w0 + 1; k < w1; k++)
            store_word(bits, row + k, 0);
        clear_word_bits(bits, row + w1, m_hi);
    }
}

// Any div_count. Rows can start mid-word when chunk_size_xy isn't a power of two, so each
// row works out its own words.
void clear_rect_any(uint8_t* bits, uint32_t div_count, uint32_t i_lo, uint32_t i_hi, uint32_t j_lo, uint32_t j_hi)
{
    for (auto lo = j_lo * div_count + i_lo, end = j_hi * div_count + i_lo; lo <= end; lo += div_count)
    {
        const auto hi = lo + (i_hi - i_lo);
        const auto w0 = lo >> 6, w1 = hi >> 6;
        const uint64_t m_lo = ~uint64_t{0} << (lo & 63);
        const uint64_t m_hi = ~uint64_t{0} >> (63 - (hi & 63));
        if (w0 == w1)
            clear_word_bits(bits, w0, m_lo & m_hi);
        else
        {
            clear_word_bits(bits, w0, m_lo);
            for (auto k = w0 + 1; k < w1; k++)
                store_word(bits, k, 0);
            clear_word_bits(bits, w1, m_hi);
        }
    }
}

// Whole 64-bit words, so the clear_rect_*() functions never read past the end of a small grid.
constexpr uint32_t bitmask_size(uint32_t div_count)
{
    return (div_count * div_count + 63) & ~63u;
}

#ifdef FLOORMAT_GRID_PASS_DEBUG
unsigned grid_total_count = 0;
unsigned grid_alive_count = 0;
unsigned grid_pooled_count = 0;
unsigned pool_total_count = 0;
unsigned pool_alive_count = 0;
#endif
} // namespace

using floormat::Grid::Pass::Params;
using floormat::Grid::Pass::pred;

struct PassGrid : GridBase
{
    using Params = Grid::Pass::Params;

    Params params;
    BitArray bitmask;
    bool all_empty = false;

    PassGrid(chunk& c, Params params);
    ~PassGrid() noexcept;

    void reset_for_reuse(chunk& ch, Params new_params);

    static uint32_t get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count);
    uint32_t get_bitmask_index_from_coord(intra_coord pos) const;
    Range2D get_coord_from_div(uint32_t x, uint32_t y) const;
    bool is_all_empty() const noexcept { return all_empty; }

    void build_impl(chunk* self, const pred& predicate);
};

uint32_t PassGrid::get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count)
{
    return GridBase::pack_bit_index(x, y, div_count);
}

uint32_t PassGrid::get_bitmask_index_from_coord(intra_coord pos) const
{
    return GridBase::pack_bit_index_from_coord(pos, params.div_count);
}

Range2D PassGrid::get_coord_from_div(uint32_t x, uint32_t y) const
{
    return GridBase::coord_range_from_div(x, y, params.div_size(), params.bbox_size);
}

void PassGrid::build_impl(chunk* self, const pred& predicate)
{
    bitmask.setAll();
    fm_debug_assert(bitmask.offset() == 0);
    uint8_t* const bits{reinterpret_cast<uint8_t*>(bitmask.data())};

    const auto div_countʹ = params.div_count;
    const auto div_size  = params.div_size();
    fm_assert(div_countʹ*div_countʹ <= bitmask.size());

    const auto half_div = (float)(div_size / 2);
    const auto half_div_minus_half_tile = half_div - half_tile<float>;
    // a box spans [center - bbox_lo, center + bbox_hi), as scenery_tile()
    const auto bbox_lo = (float)(params.bbox_size / 2), bbox_hi = (float)params.bbox_size - bbox_lo;
    // A bit must hold for every position in the cell, not just its anchor
    // a = i*div_size + half_div - half_tile. pack_bit_index_from_coord() floors, and
    // positions are integer pixels, so the cell spans [a - reach_below, a + reach_above].
    const auto reach_below = half_div;
    const auto reach_above = (float)(div_size - 1 - div_size / 2);

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
    const float inv_div = 1.f / (float)div_size;
    const int idiv_count = (int)div_countʹ;
    const auto min_pass = (uint64_t)params.min_pass;
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
                fm_debug3_assert(x.type != (uint64_t)collision_type::none);
                if (x.pass >= min_pass)
                    return true;
                auto range = Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}};
                if (predicate(*c, x, range) == path_search_continue::pass)
                    return true;
                all_empty = false;

                // interval overlap, so the cell's low end meets the obstacle's high end and vice versa
                const float bx0 = r.m_min[0] + off.x() - bbox_hi - reach_above;
                const float by0 = r.m_min[1] + off.y() - bbox_hi - reach_above;
                const float bx1 = r.m_max[0] + off.x() + bbox_lo + reach_below;
                const float by1 = r.m_max[1] + off.y() + bbox_lo + reach_below;

                // open interval (rect_intersects is strict): include j iff bx0 < j*div_size + hdmht < bx1
                int i_lo = (int)Math::floor((bx0 - half_div_minus_half_tile) * inv_div) + 1;
                int i_hi = (int)Math::ceil ((bx1 - half_div_minus_half_tile) * inv_div) - 1;
                int j_lo = (int)Math::floor((by0 - half_div_minus_half_tile) * inv_div) + 1;
                int j_hi = (int)Math::ceil ((by1 - half_div_minus_half_tile) * inv_div) - 1;

                if (i_lo < 0) i_lo = 0;
                if (j_lo < 0) j_lo = 0;
                if (i_hi >= idiv_count) i_hi = idiv_count - 1;
                if (j_hi >= idiv_count) j_hi = idiv_count - 1;
                if (i_lo > i_hi || j_lo > j_hi)
                    return true;

                clear_rect((uint32_t)i_lo, (uint32_t)i_hi, (uint32_t)j_lo, (uint32_t)j_hi);
                return true;
            });
        }
    };

    // chunk_size_xy = 1024 never reaches clear_rect_any(): every div_count is a power of two.
    if (64 % div_countʹ == 0) [[likely]]
        clear_obstacles([=](uint32_t i_lo, uint32_t i_hi, uint32_t j_lo, uint32_t j_hi) {
            clear_rect_in_word(bits, div_countʹ, i_lo, i_hi, j_lo, j_hi);
        });
    else if (div_countʹ % 64 == 0)
        clear_obstacles([=](uint32_t i_lo, uint32_t i_hi, uint32_t j_lo, uint32_t j_hi) {
            clear_rect_whole_rows(bits, div_countʹ, i_lo, i_hi, j_lo, j_hi);
        });
    else
        clear_obstacles([=](uint32_t i_lo, uint32_t i_hi, uint32_t j_lo, uint32_t j_hi) {
            clear_rect_any(bits, div_countʹ, i_lo, i_hi, j_lo, j_hi);
        });

    for (auto i = 0u; i < 8; i++)
        versions[i] = neighbors[i] ? neighbors[i]->pass_gen() : (uint64_t)-1;
    versions[8] = self->pass_gen();
}

PassGrid::PassGrid(chunk& c, Params params):
    GridBase{c},
    params{params},
    bitmask{ValueInit, bitmask_size(params.div_count)}
{
#ifdef FLOORMAT_GRID_PASS_DEBUG
    ++grid_total_count;
    ++grid_alive_count;
    DBG_nospace << "PassGrid ctor: total=" << grid_total_count << " alive=" << grid_alive_count << " pooled=" << grid_pooled_count;
#endif
}

PassGrid::~PassGrid() noexcept
{
#ifdef FLOORMAT_GRID_PASS_DEBUG
    --grid_alive_count;
    DBG_nospace << "PassGrid dtor: alive=" << grid_alive_count << " pooled=" << grid_pooled_count;
#endif
}

void PassGrid::reset_for_reuse(chunk& ch, Params new_params)
{
    reset_base_for_reuse(ch);
    params = new_params;
    const auto bits = bitmask_size(params.div_count);
    if (bitmask.size() < bits) [[unlikely]]
        bitmask = BitArray{ValueInit, bits};
    else
        bitmask.resetAll();
}

template struct Pool<PassGrid>;

struct PassPool final : Pool<PassGrid>
{
    Search::pred predicate;

    PassPool(Grid::Pass::Params params, const Search::pred& p): Pool<PassGrid>{params}, predicate{p} {}
};

namespace {
namespace Tests {

constexpr bool assert_divisor_table()
{
    constexpr auto divisors = make_divisor_table();
    fm_assert(divisors[0] == 1);
    fm_assert(divisors[array_size(divisors) - 1] < chunk_size_xy);
    fm_assert(array_size(divisors) == get_divisor_count());
    for (auto i = 0u; i < array_size(divisors); i++)
    {
        fm_assert(divisors[i] > 0);
        fm_assert(chunk_size_xy % divisors[i] == 0);
        if (i > 0)
            fm_assert(divisors[i] > divisors[i - 1]);
    }
    return true;
}

constexpr bool assert_calc_div_size()
{
    fm_assert(calc_div_size(1, 8) == 1);
    fm_assert(calc_div_size(8, 8) == 8);
    fm_assert(calc_div_size(64, 64) == 64);
    fm_assert(calc_div_size(64, 128) == 64);
    fm_assert(calc_div_size(0, chunk_size_xy) == chunk_size_xy / 2);

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

static_assert(assert_divisor_table());
static_assert(assert_calc_div_size());

} // namespace Tests
} // namespace
} // namespace floormat::detail::grid

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
    return Grid{detail::grid::pool_subscript(pool, c), pool};
}

Grid Pool::wrap(detail::grid::PassGrid* g) const noexcept { return Grid{g, pool}; }
Grid::~Grid() noexcept = default;

Grid::Grid(detail::grid::PassGrid* grid_, detail::grid::PassPool* pool_):
    grid{grid_}, pool{pool_}
{
}

uint32_t Grid::get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count)
{
    return detail::grid::PassGrid::get_bitmask_index(x, y, div_count);
}

uint32_t Grid::get_bitmask_index_from_coord(intra_coord pos) const
{
    detail::grid::check_versions(grid);
    return grid->get_bitmask_index_from_coord(pos);
}

Range2D Grid::get_coord_from_div(uint32_t x, uint32_t y) const
{
    detail::grid::check_versions(grid);
    return grid->get_coord_from_div(x, y);
}

bool Grid::bit(uint32_t index) const
{
    return detail::grid::grid_bit_at(pool, grid, index);
}

BitView Grid::bits() const
{
    detail::grid::check_versions(grid);
    fm_debug_assert(grid->bitmask.offset() == 0);
    return { reinterpret_cast<uint8_t*>(grid->bitmask.data()) };
}

uint32_t Grid::div_count() const
{
    return grid->params.div_count;
}

uint64_t Grid::build_no() const
{
    detail::grid::check_versions(grid);
    return grid->build_no;
}

bool Grid::is_all_empty() const
{
    detail::grid::check_versions(grid);
    return grid->is_all_empty();
}


void Grid::mark_stale()
{
    grid->mark_stale();
    detail::grid::cascade_mark_neighbors_stale(grid->coord, [this](chunk_coords_ ch) -> detail::grid::GridBase* {
        auto* c = grid->c->world().at(ch);
        return c ? pool->find(*c) : nullptr;
    });
}

void Grid::maybe_mark_stale()
{
    bool was_stale = grid->is_stale();
    grid->maybe_mark_stale();
    if (!was_stale && grid->is_stale())
        detail::grid::cascade_mark_neighbors_stale(grid->coord, [this](chunk_coords_ ch) -> detail::grid::GridBase* {
            auto* c = grid->c->world().at(ch);
            return c ? pool->find(*c) : nullptr;
        });
}

detail::grid::PassGrid* Grid::raw() const noexcept { return grid; }
void Grid::build_if_stale() { grid->build_if_stale(pool->predicate); }

void Pool::maybe_mark_stale_all(const world& w)
{
    if (!pool->update_checked_pass_gen(w))
        return;
    for (auto* base : pool->live)
    {
        auto* g = static_cast<detail::grid::PassGrid*>(base);
        Grid{g, pool}.maybe_mark_stale();
        g->c->ensure_passability();
        for (auto* nb : g->c->world().neighbors(g->coord))
            if (nb)
                nb->ensure_passability();
    }
}

void Pool::build_if_stale_all()
{
    for (auto* g : pool->live)
        static_cast<detail::grid::PassGrid*>(g)->build_if_stale(pool->predicate);
}

Pool::Pool(Params params, const pred& predicate): pool{new detail::grid::PassPool{params, predicate}} { }
Pool::~Pool() noexcept { delete pool; }
const Params& Pool::params() const { return pool->params; }
uint32_t Pool::pooled_count() const { return pool->freelist.size(); }

} // namespace floormat::Grid::Pass
