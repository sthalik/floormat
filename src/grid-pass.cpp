#include "grid-pass.hpp"
#include "world.hpp"
#include "collision.hpp"
#include "object.hpp"
#include "search.hpp"
#include "compat/array-size.hpp"
#include "compat/function2.hpp"
#include "compat/hash-table-load-factor.hpp"
#include <bit>
#include <array>
#include <cr/BitArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>
#include <gtl/phmap.hpp>

#include "Corrade/Utility/Path.h"

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

auto is_passable_without_critters(chunk& c) noexcept
{
    return [&c](collision_data data) {
        // XXX 'scenery' is used for all object types
        if (data.type == (uint64_t)collision_type::scenery)
        {
            auto& w = c.world();
            auto obj = w.find_object(data.id);
            fm_assert(obj);
            if (obj->type() == object_type::critter)
                return path_search_continue::pass;
        }
        return path_search_continue::blocked;
    };
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

struct Grid
{
    Grid* next = nullptr;
    Params params;
    chunk* c;
    world* w;
    std::array<chunk*, 8> neighbors{};
    std::array<uint32_t, 9> versions{};
    chunk_coords_ coord;
    uint32_t div_count;
    BitArray bitmask;

    static uint32_t get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count);
    uint32_t get_bitmask_index_from_coord(local_coords local, Vector2b offset) const;
    Range2D get_coord_from_div(uint32_t x, uint32_t y) const;

    bool is_stale() const;
    void mark_stale();
    void maybe_mark_stale();
    void build_if_stale();

    Grid(chunk& c, Params params);
    ~Grid() noexcept;
};

uint32_t Grid::get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count)
{
    auto idx = y * div_count + x;
    return idx;
}

uint32_t Grid::get_bitmask_index_from_coord(local_coords local, Vector2b offset) const
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

Range2D Grid::get_coord_from_div(uint32_t x, uint32_t y) const
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

bool Grid::is_stale() const
{
    return versions[8] == (uint32_t)-1;
}

void Grid::mark_stale()
{
    versions[8] = (uint32_t)-1;
}

void Grid::maybe_mark_stale()
{
    auto* current = w->at(coord);
    if (c != current)
    {
        c = current;
        mark_stale();
        return;
    }

    if (is_stale())
        return;

    if (current && current->is_passability_modified())
    {
        mark_stale();
        return;
    }

    auto cur_ver = current ? current->pass_gen_counter() : (uint32_t)-1;
    if (versions[8] != cur_ver)
    {
        mark_stale();
        return;
    }

    for (auto i = 0u; i < 8; i++)
    {
        auto* nb = w->at(coord + world::neighbor_offsets[i]);

        if (nb != neighbors[i])
        {
            neighbors[i] = nb;
            mark_stale();
            return;
        }
        if (nb && nb->is_passability_modified())
        {
            mark_stale();
            return;
        }
        auto nb_ver = nb ? nb->pass_gen_counter() : (uint32_t)-1;
        if (nb_ver != versions[i])
        {
            mark_stale();
            return;
        }
    }
}

void Grid::build_if_stale()
{
    if (!is_stale())
        return;

    auto* self = w->at(coord);
    fm_assert(self);

    neighbors = w->neighbors(coord);
    for (auto i = 0u; i < 8; i++)
        versions[i] = neighbors[i] ? neighbors[i]->pass_gen_counter() : (uint32_t)-1;
    versions[8] = self->pass_gen_counter();
    bitmask.resetAll();
    fm_debug_assert(bitmask.offset() == 0);
    uint8_t* const bits{reinterpret_cast<uint8_t*>(bitmask.data())};

    const auto div_countʹ = div_count;
    const auto div_size  = params.div_size;
    const auto idx = div_countʹ * (div_countʹ-1) + (div_countʹ-1);
    fm_assert(idx < div_countʹ*div_countʹ);

    const auto function = is_passable_without_critters(*self);
    const auto half = (float)params.bbox_size*.5f;
    constexpr auto half_tile = tile_size_xy*.5f;
    const auto half_div = (float)(div_size / 2);
    const auto half_div_minus_half_tile = half_div - half_tile;

    for (auto j = 0u; j < div_countʹ; j++)
    {
        const auto py = j * div_size + half_div_minus_half_tile;
        const auto sy = (float)py - half;
        const auto ey = (float)py + half;
        const auto by = j * div_countʹ;

        for (auto i = 0u; i < div_countʹ; i++)
        {
            const auto px = i * div_size + half_div_minus_half_tile;
            const auto sx = (float)px - half;
            const auto ex = (float)px + half;
            const auto ss = Vector2{sx, sy};
            const auto ee = Vector2{ex, ey};
            const bool value = path_search::is_passable_(self, neighbors, ss, ee, 0, function);
            const auto bit   = by + i;
            bits[bit >> 3] |= uint8_t(value) << (bit & 7);
        }
    }
}

Grid::Grid(chunk& c, Params params):
    params{params},
    c{&c},
    w{&c.world()},
    coord{c.coord()},
    div_count{chunk_size_xy / params.div_size},
    bitmask{ValueInit, div_count*div_count}
{
    versions.fill((uint32_t)-1);
#ifdef FLOORMAT_GRID_PASS_DEBUG
    ++grid_total_count;
    ++grid_alive_count;
    DBG_nospace << "Grid ctor: total=" << grid_total_count << " alive=" << grid_alive_count << " pooled=" << grid_pooled_count;
#endif
}

Grid::~Grid() noexcept
{
#ifdef FLOORMAT_GRID_PASS_DEBUG
    --grid_alive_count;
    DBG_nospace << "Grid dtor: alive=" << grid_alive_count << " pooled=" << grid_pooled_count;
#endif
}

template <typename T>
class free_list
{
    T* list = nullptr;

public:
    T* take();
    void put(T* p);

    bool is_empty() const;
    uint32_t size() const;
    static T*& next(T* p);
};

template <typename T> bool free_list<T>::is_empty() const { return !list; }

template <typename T> uint32_t free_list<T>::size() const
{
    uint32_t n = 0;
    for (auto* p = list; p; p = next(p))
        n++;
    return n;
}

template <typename T> void free_list<T>::put(T* p)
{
    next(p) = list;
    list = p;
}

template <typename T> T* free_list<T>::take()
{
    auto* p = list;
    if (p)
    {
        list = next(p);
        next(p) = nullptr;
    }
    return p;
}

template<> Grid*& free_list<Grid>::next(Grid* grid) { return grid->next; }

struct Pool
{
    class free_list<Grid> free_list;
    gtl::flat_hash_map<chunk_coords_, Grid*, Hash::chunk_coord_hasher> grids{};
    Params params;
    uint64_t frame_no = (uint64_t)-1;

    explicit Pool(Params params);
    ~Pool() noexcept;
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Pool);

    void put(Grid* p);
    Grid* take(chunk& ch);
};

Pool::~Pool() noexcept
{
    for (auto& [_, g] : grids)
        delete g;
    while (auto* g = free_list.take())
    {
        fm_assert(!g->c);
#ifdef FLOORMAT_GRID_PASS_DEBUG
        --grid_pooled_count;
#endif
        delete g;
    }
#ifdef FLOORMAT_GRID_PASS_DEBUG
    --pool_alive_count;
    DBG_nospace << "Pool dtor: alive=" << pool_alive_count << " pooled_grids=" << grid_pooled_count;
#endif
}

Pool::Pool(Params param):
    params{param.validate()}
{
    Hash::set_open_addressing_load_factor(grids);
#ifdef FLOORMAT_GRID_PASS_DEBUG
    ++pool_total_count;
    ++pool_alive_count;
    DBG_nospace << "Pool ctor: total=" << pool_total_count << " alive=" << pool_alive_count << " pooled_grids=" << grid_pooled_count;
#endif
}

void Pool::put(Grid* p)
{
    fm_assert(p);
    fm_assert(!p->c);
    const auto div_size = p->params.div_size;
    const auto div_count = chunk_size_xy / div_size;
    fm_debug2_assert(p->div_count == div_count);
    fm_assert(div_count*div_count <= p->bitmask.size());
#ifdef FLOORMAT_GRID_PASS_DEBUG
    ++grid_pooled_count;
    DBG_nospace << "Pool put: alive=" << grid_alive_count << " pooled=" << grid_pooled_count;
#endif
    free_list.put(p);
}

Grid* Pool::take(chunk& ch)
{
    if (auto p = free_list.take())
    {
#ifdef FLOORMAT_GRID_PASS_DEBUG
        --grid_pooled_count;
        DBG_nospace << "Pool take (reuse): alive=" << grid_alive_count << " pooled=" << grid_pooled_count;
#endif
        p->params = params;
        p->c = &ch;
        p->w = &ch.world();
        p->neighbors = {};
        p->versions.fill((uint32_t)-1);
        p->coord = ch.coord();
        p->div_count = chunk_size_xy / params.div_size;
        const auto bits = p->div_count * p->div_count;
        if (p->bitmask.size() < bits) [[unlikely]]
            p->bitmask = BitArray{ValueInit, bits};
        else
            p->bitmask.resetAll();
        return p;
    }
    else
        return new Grid{ch, params};
}

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

    // every divisor round-trips to its index
    for (auto i = 0u; i < array_size(divisors); i++)
        fm_assert(inv[divisors[i] - 1] == (T)i);

    // every non-divisor stays sentinel
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
    // valid input: pass through unchanged
    fm_assert(calc_div_size(1, 8) == 1);
    fm_assert(calc_div_size(8, 8) == 8);
    fm_assert(calc_div_size(64, 64) == 64);
    fm_assert(calc_div_size(64, 128) == 64);
    fm_assert(calc_div_size(chunk_size_xy, chunk_size_xy) == chunk_size_xy);

    // div_size=0: snap to largest divisor ≤ bbox
    fm_assert(calc_div_size(0, 64) == 64);
    fm_assert(calc_div_size(0, 100) == 64); // 100 isn't a divisor; largest ≤ 100 is 64
    fm_assert(calc_div_size(0, 8) == 8);

    // div_size > bbox: clamp to largest divisor ≤ bbox
    fm_assert(calc_div_size(1024, 64) == 64);
    fm_assert(calc_div_size(128, 100) == 64);

    // div_size doesn't divide chunk_size_xy: snap down
    fm_assert(calc_div_size(100, 200) == 64);
    fm_assert(calc_div_size(3, 8) == 2);
    fm_assert(calc_div_size(7, 8) == 4);

    // output is always a valid divisor, always ≤ bbox, always > 0
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

bool BitView::read(uint32_t i) const
{
    return data[i >> 3] & (1u << (i & 7));
}

void BitView::set(uint32_t i)
{
    data[i >> 3] |= uint8_t(1u << (i & 7));
}

void BitView::reset(uint32_t i)
{
    data[i >> 3] &= uint8_t(~(1u << (i & 7)));
}

void BitView::write(uint32_t i, bool value)
{
    auto& byte = data[i >> 3];
    auto mask = uint8_t(1u << (i & 7));
    byte = uint8_t((byte & ~mask) | (uint8_t(value) << (i & 7)));
}

bool Params::operator==(const Params&) const noexcept = default;

Params Params::validate() const
{
    using detail::grid::calc_div_size;
    constexpr uint32_t min_bbox_size = 4;

    auto bbox_sizeʹ = Math::max(min_bbox_size, bbox_size);
    auto div_sizeʹ  = calc_div_size(div_size, bbox_sizeʹ);
    Params p{ div_sizeʹ, bbox_sizeʹ, };
    fm_assert(p.div_size > 0);
    fm_assert(chunk_size_xy % p.div_size == 0);
    fm_assert(p.bbox_size >= p.div_size);
    fm_assert(p.bbox_size <= chunk_size_xy);
    return p;
}

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

Grid::Grid(detail::grid::Grid* grid_, detail::grid::Pool* pool_):
    grid{grid_}, pool{pool_}
{
}

uint32_t Grid::get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count)
{
    return detail::grid::Grid::get_bitmask_index(x, y, div_count);
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

Grid::Grid(const Grid&) noexcept = default;
Grid& Grid::operator=(const Grid&) & noexcept = default;

namespace {
void cascade_mark_neighbors_stale(detail::grid::Pool* pool, chunk_coords_ coord)
{
    constexpr Vector2i offsets[] {
        {-1,-1}, { 0,-1}, { 1,-1},
        {-1, 0},          { 1, 0},
        {-1, 1}, { 0, 1}, { 1, 1},
    };
    for (auto off : offsets)
    {
        auto nb = coord + off;
        auto it = pool->grids.find(nb, nb.hash());
        if (it != pool->grids.end())
            it->second->mark_stale();
    }
}
} // namespace

void Grid::mark_stale()
{
    grid->mark_stale();
    cascade_mark_neighbors_stale(pool, grid->coord);
}

void Grid::maybe_mark_stale()
{
    bool was_stale = grid->is_stale();
    grid->maybe_mark_stale();
    if (!was_stale && grid->is_stale())
        cascade_mark_neighbors_stale(pool, grid->coord);
}

Grid::~Grid() noexcept = default;
void Grid::build_if_stale() { grid->build_if_stale(); }

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

void Pool::build_if_stale_all()
{
    for (auto [k, grid] : pool->grids)
        grid->build_if_stale();
}

Pool::Pool(Params params): pool{new detail::grid::Pool{params}} { }
Pool::~Pool() noexcept { delete pool; }
Params Pool::params() const { return pool->params; }
uint32_t Pool::pooled_count() const { return pool->free_list.size(); }

} // namespace floormat::Grid::Pass
