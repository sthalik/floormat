#include "grid-pass.hpp"
#include "grid.inl"
#include "world.hpp"
#include "collision.hpp"
#include "object.hpp"
#include "search.hpp"
#include "search-cache.hpp"
#include "compat/array-size.hpp"
#include "compat/function2.hpp"
#include "compat/hash-table-load-factor.hpp"
#include <bit>
#include <array>
#include <cr/BitArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>
#include <gtl/phmap.hpp>

//#define FLOORMAT_GRID_PASS_DEBUG 1

namespace floormat {
namespace { constexpr inline auto chunk_size_xy = tile_size_xy*TILE_MAX_DIM; } // namespace
} // namespace floormat

namespace floormat::detail::grid {
namespace {
constexpr auto get_divisor_count()
{
    auto count = 0u;
    for (auto i = 1u; i <= chunk_size_xy; i++)
        if (chunk_size_xy % i == 0)
            count++;
    return count;
}

constexpr auto make_divisor_table()
{
    constexpr auto count = get_divisor_count();
    std::array<uint32_t, count> array;
    uint32_t divisor = 1;
    for (auto i = 0u; i < array_size(array); i++)
    {
        while (chunk_size_xy % divisor)
            divisor++;
        array[i] = divisor++;
    }
    return array;
}

constexpr auto make_inverse_divisor_table()
{
    constexpr auto divisors = make_divisor_table();
    using integer_type =
        std::conditional_t<(array_size(divisors) > 255),
                           std::conditional_t<(array_size(divisors) > 65535), uint32_t, uint16_t>,
                           uint8_t>;

    std::array<integer_type, chunk_size_xy> array;

    for (auto& x : array)
        x = (integer_type)-1;

    for (auto i = 0u; i < array_size(divisors); i++)
    {
        auto div = divisors[i];
        fm_assert(div > 0);
        fm_assert(div - 1 < array_size(array));
        array[div - 1] = (integer_type)i;
    }

    return array;
}

constexpr auto get_divisor_index(uint32_t div_size)
{
    constexpr auto inv = make_inverse_divisor_table();
    fm_assert(div_size > 0 && div_size <= chunk_size_xy);
    auto i = inv[div_size - 1];
    if (i == (decltype(i))-1) [[unlikely]]
        fm_abort("get_divisor_index: wrong div_size %u", (unsigned)div_size);
    return (uint32_t)i;
}

constexpr auto calc_div_size(uint32_t div_sizeʹ, uint32_t bbox_size)
{
    constexpr auto div_sizes = make_divisor_table();
    uint32_t div_size = div_sizeʹ;

    if (!div_sizeʹ || div_sizeʹ > bbox_size) [[unlikely]]
    {
        // caller wants cells coarser than the bbox; grid requires div_size <= bbox_size
        div_size = div_sizes[0];  // smallest divisor as floor
        for (auto i : div_sizes)
            if (i <= bbox_size)
                div_size = i;
            else
                break; // largest ≤ upper
    }
    else if (chunk_size_xy % div_sizeʹ) [[unlikely]]
    {
        // valid magnitude but it doesn't divide the chunk evenly; snap down so cells tile the chunk
        div_size = div_sizes[0];
        for (auto i : div_sizes)
            if (i <= div_sizeʹ)
                div_size = i;
            else
                break;
    }
    return div_size;
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
    uint32_t div_count;
    BitArray bitmask;

    PassGrid(chunk& c, Params params);
    ~PassGrid() noexcept;

    void reset_for_reuse(chunk& ch, Params new_params);

    static uint32_t get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count);
    uint32_t get_bitmask_index_from_coord(local_coords local, Vector2b offset) const;
    Range2D get_coord_from_div(uint32_t x, uint32_t y) const;

    void build_impl(chunk* self, const pred& predicate);
};

uint32_t PassGrid::get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count)
{
    return GridBase::pack_bit_index(x, y, div_count);
}

uint32_t PassGrid::get_bitmask_index_from_coord(local_coords local, Vector2b offset) const
{
    constexpr auto half_tile = tile_size_xy/2;
    Vector2i posʹ;
    posʹ += Vector2i(local) * tile_size_xy;
    posʹ += Vector2i(offset);
    posʹ += Vector2i(half_tile);
    fm_debug2_assert(posʹ >= Vector2i{0});
    Vector2ui pos{NoInit}; (void)pos;
    if constexpr (std::has_single_bit(uint32_t{chunk_size_xy}))
        pos = Vector2ui(posʹ) >> (uint32_t)std::countr_zero(params.div_size);
    else
        pos = Vector2ui(posʹ) / params.div_size;
    auto idx = get_bitmask_index(pos.x(), pos.y(), div_count);
    fm_assert(idx < div_count*div_count);
    return idx;
}

Range2D PassGrid::get_coord_from_div(uint32_t x, uint32_t y) const
{
    constexpr auto half_tile = Vector2i{tile_size_xy/2};
    const auto bbox_size = Vector2(params.bbox_size);
    const auto half_bbox = bbox_size*.5f;
    auto pos = Vector2i{(int32_t)x, (int32_t)y};
    pos *= Vector2i{(int32_t)params.div_size};
    pos += Vector2i(params.div_size / 2);
    pos -= half_tile;
    fm_debug_assert(pos >= -half_tile);
    fm_debug_assert(pos < Vector2i((int32_t)chunk_size_xy) - half_tile);
    auto posʹ = Vector2(pos);
    auto min = posʹ - half_bbox;
    auto max = min + bbox_size;
    return { min, max };
}

void PassGrid::build_impl(chunk* self, const pred& predicate)
{
    bitmask.setAll();
    fm_debug_assert(bitmask.offset() == 0);
    uint8_t* const bits{reinterpret_cast<uint8_t*>(bitmask.data())};

    const auto div_countʹ = div_count;
    const auto div_size  = params.div_size;
    fm_assert(div_countʹ*div_countʹ <= bitmask.size());

    // +div_size: bit must hold for any critter position in the cell, not just the centre
    const auto half = ((float)params.bbox_size + (float)div_size) * .5f;
    constexpr auto half_tile = tile_size_xy*.5f;
    const auto half_div = (float)(div_size / 2);
    const auto half_div_minus_half_tile = half_div - half_tile;

    chunk* const chunks[9] = {
        self,
        neighbors[0], neighbors[1], neighbors[2], neighbors[3],
        neighbors[4], neighbors[5], neighbors[6], neighbors[7],
    };
    static constexpr auto nb_offsets = []() {
        constexpr float chunk_size = (float)tile_size_xy * (float)TILE_MAX_DIM;
        std::array<Vector2, 9> a{};
        for (auto i = 0u; i < 8; i++)
            a[i+1] = Vector2(world::neighbor_offsets[i]) * chunk_size;
        return a;
    }();
    static constexpr auto ch_offsets = []() {
        std::array<Vector2b, 9> a{};
        for (auto i = 0u; i < 8; i++)
            a[i+1] = world::neighbor_offsets[i];
        return a;
    }();

    // cull radius: max world distance from a cell to an obstacle for collision.
    // = obstacle half (127.5) + critter half (127.5) + cell pad (div_size/2)
    const auto cull_radius = 255.f + (float)div_size * .5f;
    const auto K = Math::min(div_countʹ, (uint32_t)Math::ceil(cull_radius / (float)div_size));
    const uint32_t lo[3] = { 0u, 0u, div_countʹ - K };
    const uint32_t hi[3] = { K,  div_countʹ, div_countʹ };

    for (auto n = 0u; n < 9; n++)
    {
        auto* c = chunks[n];
        if (!c)
            continue;
        const auto off = nb_offsets[n];
        const auto sx_lo = lo[ch_offsets[n].x() + 1], sx_hi = hi[ch_offsets[n].x() + 1];
        const auto sy_lo = lo[ch_offsets[n].y() + 1], sy_hi = hi[ch_offsets[n].y() + 1];

        for (auto j = sy_lo; j < sy_hi; j++)
        {
            const auto py = (float)(j * div_size) + half_div_minus_half_tile;
            const auto sy = py - half - off.y();
            const auto ey = py + half - off.y();
            const auto by = j * div_countʹ;

            for (auto i = sx_lo; i < sx_hi; i++)
            {
                const auto bit = by + i;
                const auto mask = uint8_t(1u << (bit & 7));
                if (!(bits[bit >> 3] & mask))
                    continue;
                const auto px = (float)(i * div_size) + half_div_minus_half_tile;
                const auto sx = px - half - off.x();
                const auto ex = px + half - off.x();
                if (!path_search::is_passable_1(*c, {sx, sy}, {ex, ey}, predicate))
                    bits[bit >> 3] = uint8_t(bits[bit >> 3] & ~mask);
            }
        }
    }

    for (auto i = 0u; i < 8; i++)
        versions[i] = neighbors[i] ? neighbors[i]->pass_gen_counter() : (uint32_t)-1;
    versions[8] = self->pass_gen_counter();
}

PassGrid::PassGrid(chunk& c, Params params):
    GridBase{c},
    params{params},
    div_count{chunk_size_xy / params.div_size},
    bitmask{ValueInit, div_count*div_count}
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
    div_count = chunk_size_xy / params.div_size;
    const auto bits = div_count * div_count;
    if (bitmask.size() < bits) [[unlikely]]
        bitmask = BitArray{ValueInit, bits};
    else
        bitmask.resetAll();
}

template struct Pool<PassGrid>;

namespace {
namespace Tests {

constexpr bool assert_divisor_table()
{
    constexpr auto divisors = make_divisor_table();
    fm_assert(divisors[0] == 1);
    fm_assert(divisors[array_size(divisors) - 1] == chunk_size_xy);
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

constexpr bool assert_inverse_divisor_table()
{
    constexpr auto divisors = make_divisor_table();
    constexpr auto inv = make_inverse_divisor_table();
    using T = std::remove_cvref_t<decltype(inv[0])>;
    constexpr T sentinel = (T)-1;

    for (auto i = 0u; i < array_size(divisors); i++)
        fm_assert(inv[divisors[i] - 1] == (T)i);

    for (auto n = 1u; n <= chunk_size_xy; n++)
        if (chunk_size_xy % n != 0)
            fm_assert(inv[n - 1] == sentinel);

    return true;
}

constexpr bool assert_get_divisor_index()
{
    constexpr auto divisors = make_divisor_table();
    for (auto i = 0u; i < array_size(divisors); i++)
        fm_assert(get_divisor_index(divisors[i]) == i);
    return true;
}

constexpr bool assert_calc_div_size()
{
    fm_assert(calc_div_size(1, 8) == 1);
    fm_assert(calc_div_size(8, 8) == 8);
    fm_assert(calc_div_size(64, 64) == 64);
    fm_assert(calc_div_size(64, 128) == 64);
    fm_assert(calc_div_size(chunk_size_xy, chunk_size_xy) == chunk_size_xy);

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
static_assert(assert_inverse_divisor_table());
static_assert(assert_get_divisor_index());
static_assert(assert_calc_div_size());

} // namespace Tests
} // namespace
} // namespace floormat::detail::grid

namespace floormat::Grid::Pass {

namespace {
constexpr auto without_critters_lambda = [](chunk& self, collision_data data) -> path_search_continue {
    // 'scenery' covers all object types here, including critters
    if (data.type == (uint64_t)collision_type::scenery)
    {
        auto obj = self.world().find_object(data.id);
        fm_assert(obj);
        if (obj->type() == object_type::critter)
            return path_search_continue::pass;
    }
    return path_search_continue::blocked;
};
constexpr pred without_critters_pred{without_critters_lambda};
} // namespace

const pred& is_passable_without_critters() { return without_critters_pred; }

Params Params::validate() const
{
    using detail::grid::calc_div_size;
    constexpr uint32_t min_bbox_size = 8;

    auto bbox_sizeʹ = Math::max(min_bbox_size, bbox_size);
    auto div_sizeʹ  = calc_div_size(div_size, bbox_sizeʹ);
    Params p{ div_sizeʹ, bbox_sizeʹ, };
    fm_assert(p.div_size > 0);
    fm_assert(chunk_size_xy % p.div_size == 0);
    fm_assert(p.bbox_size >= p.div_size);
    fm_assert(p.bbox_size <= chunk_size_xy);
    return p;
}

bool Params::operator==(const Params&) const noexcept = default;

Grid Pool::operator[](chunk& c)
{
    fm_assert(pool->frame_no == c.world().frame_no());
    Hash::set_open_addressing_load_factor(pool->grids, pool->grids.size() + 1);
    auto [it, inserted] = pool->grids.try_emplace(c.coord(), nullptr);
    if (inserted)
        it->second = pool->take(c);
    else
        fm_debug2_assert(it->second);
    return Grid{it->second, pool};
}

Grid Pool::wrap(detail::grid::PassGrid* g) const noexcept { return Grid{g, pool}; }

Grid::Grid(detail::grid::PassGrid* grid_, detail::grid::Pool<detail::grid::PassGrid>* pool_):
    grid{grid_}, pool{pool_}
{
}

uint32_t Grid::get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count)
{
    return detail::grid::PassGrid::get_bitmask_index(x, y, div_count);
}

uint32_t Grid::get_bitmask_index_from_coord(local_coords local, Vector2b offset) const
{
    fm_assert(pool->frame_no == grid->w->frame_no());
    return grid->get_bitmask_index_from_coord(local, offset);
}

Range2D Grid::get_coord_from_div(uint32_t x, uint32_t y) const
{
    fm_assert(pool->frame_no == grid->w->frame_no());
    return grid->get_coord_from_div(x, y);
}

bool Grid::bit(uint32_t index) const
{
    fm_assert(pool->frame_no == grid->w->frame_no());
    fm_assert(index < grid->bitmask.size());
    return grid->bitmask[index];
}

BitView Grid::bits() const
{
    fm_assert(pool->frame_no == grid->w->frame_no());
    fm_debug_assert(grid->bitmask.offset() == 0);
    return { reinterpret_cast<uint8_t*>(grid->bitmask.data()) };
}

uint32_t Grid::div_count() const
{
    return grid->div_count;
}


void Grid::mark_stale()
{
    grid->mark_stale();
    detail::grid::cascade_mark_neighbors_stale(grid->coord, [this](chunk_coords_ ch) -> detail::grid::GridBase* {
        auto it = pool->grids.find(ch, ch.hash());
        return it != pool->grids.end() ? it->second : nullptr;
    });
}

void Grid::maybe_mark_stale()
{
    bool was_stale = grid->is_stale();
    grid->maybe_mark_stale();
    if (!was_stale && grid->is_stale())
        detail::grid::cascade_mark_neighbors_stale(grid->coord, [this](chunk_coords_ ch) -> detail::grid::GridBase* {
            auto it = pool->grids.find(ch, ch.hash());
            return it != pool->grids.end() ? it->second : nullptr;
        });
}

void Grid::maybe_mark_stale(Search::cache& cache)
{
    bool was_stale = grid->is_stale();
    grid->maybe_mark_stale(cache);
    if (!was_stale && grid->is_stale())
        detail::grid::cascade_mark_neighbors_stale(grid->coord, [this](chunk_coords_ ch) -> detail::grid::GridBase* {
            auto it = pool->grids.find(ch, ch.hash());
            return it != pool->grids.end() ? it->second : nullptr;
        });
}

Grid::operator bool() const noexcept { return grid != nullptr; }
detail::grid::PassGrid* Grid::raw() const noexcept { return grid; }
void Grid::build_if_stale(const pred& predicate) { grid->build_if_stale(predicate); }
void Grid::build_if_stale(Search::cache& cache, const pred& predicate) { grid->build_if_stale(cache, predicate); }

void Pool::maybe_mark_stale_all(uint64_t frame_no)
{
    pool->frame_no = frame_no;
    for (auto it = pool->grids.begin(); it != pool->grids.end(); )
    {
        auto* g = it->second;
        Grid{g, pool}.maybe_mark_stale();
        if (!g->c)
        {
            it = pool->grids.erase(it);
            pool->put(g);
        }
        else
        {
            ++it;

            g->c->ensure_passability();
            for (auto* nb : g->w->neighbors(g->coord))
                if (nb)
                    nb->ensure_passability();
        }
    }
}

void Pool::maybe_mark_stale_all(uint64_t frame_no, Search::cache& cache)
{
    pool->frame_no = frame_no;
    for (auto it = pool->grids.begin(); it != pool->grids.end(); )
    {
        auto* g = it->second;
        const bool in_rect = cache.contains_chunk(g->coord);

        if (in_rect)
            Grid{g, pool}.maybe_mark_stale(cache);
        else
            Grid{g, pool}.maybe_mark_stale();

        if (!g->c)
        {
            it = pool->grids.erase(it);
            pool->put(g);
        }
        else
        {
            ++it;

            g->c->ensure_passability();
            if (in_rect)
            {
                for (auto* nb : cache.get_neighbors(*g->w, g->coord))
                    if (nb)
                        nb->ensure_passability();
            }
            else
            {
                for (auto* nb : g->w->neighbors(g->coord))
                    if (nb)
                        nb->ensure_passability();
            }
        }
    }
}

void Pool::build_if_stale_all(const pred& predicate)
{
    for (auto [k, grid] : pool->grids)
        grid->build_if_stale(predicate);
}

void Pool::build_if_stale_all(Search::cache& cache, const pred& predicate)
{
    for (auto [k, grid] : pool->grids)
    {
        if (cache.contains_chunk(k))
            grid->build_if_stale(cache, predicate);
        else
            grid->build_if_stale(predicate);
    }
}

Pool::Pool(Params params): pool{new detail::grid::Pool<detail::grid::PassGrid>{params}} { }
Pool::~Pool() noexcept { delete pool; }
Params Pool::params() const { return pool->params; }
uint32_t Pool::pooled_count() const { return pool->freelist.size(); }

} // namespace floormat::Grid::Pass
