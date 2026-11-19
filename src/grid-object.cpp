#include "grid-object.hpp"
#include "grid.inl"
#include "intra-coord.hpp"
#include "world.hpp"
#include "critter.hpp"
#include "scenery.hpp"
#include "tile-bbox.hpp"
#include "object-storage.inl"
#include "compat/function-view.hpp"
#include <cstring>
#include <cr/GrowableArray.h>

namespace floormat::Grid::detail {

namespace {

template<typename T> constexpr inline T run_bit = (T)(1u << (sizeof(T)*8 - 1));
template<typename T> constexpr inline T empty_run = run_bit<T>;
template<typename T> constexpr inline u32 max_entries = run_bit<T> - 1u;
template<typename T> constexpr inline u32 max_run_entries = run_bit<T>;

template<typename T> void reserve_scratch(Array<T>& A, u32 count)
{
    if (arrayCapacity(A) < count)
        arrayReserve(A, count + count/2);
}

} // namespace

using floormat::Grid::Object::Params;

struct ObjectGrid : GridBase
{
    using Params = Grid::Object::Params;

    Params params;
    // A cell holds its one entry's index, or with run_bit set the offset in runs of a count and that many indices.
    // Empty cells, those past div_count² included, hold empty_run, and runs[0] is a count of 0.
    Array<u16> cells;
    Array<u16> runs;
    Array<u32> cells32;
    Array<u32> runs32;
    Array<collision_data> entry_data;
    Array<u8> entry_chunks;
    // Kept until the next build clears the cells this one wrote.
    Array<cell_range> entry_ranges;
    bool uses_u32 = false, build_u32_next = false;

    ObjectGrid(chunk& c, Params params);

    void reset_for_reuse(chunk& ch, Params new_params);
    void clear_cells();
    template<typename T> void clear_cells(Array<T>& cells, u32 range_count);
    template<typename T> bool is_blocked(const Array<T>& cells, const Array<T>& runs, u32 index,
                                         ArrayView<const bool> entry_blocked) const;
    template<typename T> void mask_pass_bits(const Array<T>& cells, const Array<T>& runs, const usize* pass_bits,
                                             usize* out, ArrayView<const bool> entry_blocked) const;
    template<typename T> Grid::Object::cell_object_view<T> objects_at_cell(const Array<T>& cells,
                                                                           const Array<T>& runs, u32 index) const;
    bool is_entry_blocked(u32 k, const Search::pred& p) const;
    bool objects_maybe_dirty() const;
    // A new objects_gen() value marks no grid stale. objects_maybe_dirty() compares the 9 chunks' values
    // instead, so moving a critter writes to no grid.
    bool needs_rebuild() const { return is_marked_stale() || objects_maybe_dirty(); }
    void build_impl(chunk* self);
    template<typename T> bool build_impl(Array<T>& cells, Array<T>& runs, Array<u32>& scratch_runs);
};

struct ObjectPool final : Pool<ObjectGrid>
{
    Pass::Pool& pass_pool;
    Array<u32> scratch_runs;

    explicit ObjectPool(Pass::Pool& pass_pool): Pool<ObjectGrid>{pass_pool.params()}, pass_pool{pass_pool}
    {
        arrayReserve(scratch_runs, 16);
    }
};

ObjectGrid::ObjectGrid(chunk& c, Params params):
    GridBase{c},
    params{params},
    cells{DirectInit, bitmask_size(params.div_count), empty_run<u16>}
{
    arrayReserve(runs, 16);
    arrayResize(runs, ValueInit, 1);
    arrayReserve(runs32, 16);
    arrayReserve(entry_data, 16);
    arrayReserve(entry_chunks, 16);
    arrayReserve(entry_ranges, 16);
}

void ObjectGrid::reset_for_reuse(chunk& ch, Params new_params)
{
    clear_cells();
    reset_base_for_reuse(ch);
    params = new_params;
    if (const auto n = bitmask_size(params.div_count); cells.size() < n) [[unlikely]]
        cells = Array<u16>{DirectInit, n, empty_run<u16>};
    arrayClear(entry_data);
    arrayClear(entry_chunks);
    uses_u32 = build_u32_next = false;
}

void ObjectGrid::clear_cells()
{
    if (uses_u32)
        clear_cells(cells32, (u32)entry_ranges.size());
    else
        clear_cells(cells, (u32)entry_ranges.size());
    arrayClear(entry_ranges);
}

template<typename T> void ObjectGrid::clear_cells(Array<T>& cells, u32 range_count)
{
    const auto dc = params.div_count;
    for (auto k = 0u; k < range_count; k++)
    {
        const auto& r = entry_ranges[k];
        for (auto j = (u32)r.j_lo; j <= (u32)r.j_hi; j++)
            for (auto i = (u32)r.i_lo; i <= (u32)r.i_hi; i++)
                cells[j*dc + i] = empty_run<T>;
    }
}

template<typename T>
bool ObjectGrid::is_blocked(const Array<T>& cells, const Array<T>& runs, u32 index,
                            ArrayView<const bool> entry_blocked) const
{
    const auto v = cells.data()[index];
    if (!(v & run_bit<T>))
        return entry_blocked[v];
    const auto* r = runs.data() + (v & ~run_bit<T>);
    for (auto k = 1u; k <= r[0]; k++)
        if (entry_blocked[r[k]])
            return true;
    return false;
}

template<typename T>
void ObjectGrid::mask_pass_bits(const Array<T>& cells, const Array<T>& runs, const usize* pass_bits, usize* out,
                                ArrayView<const bool> entry_blocked) const
{
    const auto n = bitmask_size(params.div_count) / (u32)(sizeof(usize)*8);
    for (auto k = 0u; k < n; k++)
    {
        usize mask = 0;
        for (auto t = 0u; t < sizeof(usize)*8; t++)
            mask |= (usize)!is_blocked(cells, runs, k*(u32)(sizeof(usize)*8) + t, entry_blocked) << t;
        out[k] = pass_bits[k] & mask;
    }
}

template<typename T>
Grid::Object::cell_object_view<T> ObjectGrid::objects_at_cell(const Array<T>& cells, const Array<T>& runs,
                                                              u32 index) const
{
    const auto* v = cells.data() + index;
    if (!(*v & run_bit<T>))
        return { v, entry_data.data(), 1 };
    const auto* r = runs.data() + (*v & ~run_bit<T>);
    return { r + 1, entry_data.data(), r[0] };
}

bool ObjectGrid::is_entry_blocked(u32 k, const Search::pred& p) const
{
    const auto n = entry_chunks[k];
    auto& ch = n ? *neighbors[n - 1] : *c;
    return p(ch, entry_data[k]) == path_search_continue::blocked;
}

bool ObjectGrid::objects_maybe_dirty() const
{
    if (c->objects_gen() > built_pass_gen)
        return true;
    for (const auto* nb : neighbors)
        if (nb && nb->objects_gen() > built_pass_gen)
            return true;
    return false;
}

void ObjectGrid::build_impl(chunk* self)
{
    const auto dc = params.div_count;
    fm_assert(dc*dc <= cells.size());
    auto& scratch_runs = static_cast<ObjectPool*>(pool)->scratch_runs;
    clear_cells();
    arrayClear(entry_data);
    arrayClear(entry_chunks);

    const inflated_cells cells_of{params};
    const auto min_pass = params.min_pass;
    chunk* const chunks[9] = {
        self,
        neighbors[0], neighbors[1], neighbors[2], neighbors[3],
        neighbors[4], neighbors[5], neighbors[6], neighbors[7],
    };

    for (auto n = 0u; n < 9; n++)
    {
        auto* ch = chunks[n];
        if (!ch)
            continue;
        const auto off = n ? Vector2i(world::neighbor_offsets[n-1]) * chunk_size<i32> : Vector2i{};
        const auto add = [&]<typename T>(const object_list<T>& list) {
            for (const T& o : list)
            {
                if (!Search::is_dynamic_object(o) || o.pass >= min_pass || Vector2ui(o.bbox_size).product() == 0)
                    continue;
                const auto box = scenery_tile(o.coord.local(), o.offset, o.bbox_offset, o.bbox_size);
                const auto min = box.first() + off, max = box.second() + off;
                const auto r = cells_of((float)min.x(), (float)min.y(), (float)max.x(), (float)max.y());
                if (r.is_empty())
                    continue;
                arrayAppend(entry_data, collision_data{(u64)collision_type::scenery, (u64)o.pass, o.id});
                arrayAppend(entry_chunks, (u8)n);
                arrayAppend(entry_ranges, r);
            }
        };
        const auto& objects = ch->objects();
        add(objects.list<critter>());
        add(objects.list<door_scenery>());
        add(objects.list<generic_scenery>());
    }

    uses_u32 = build_u32_next;
    if (!uses_u32 && !build_impl(cells, runs, scratch_runs)) [[unlikely]]
        uses_u32 = build_u32_next = true;
    if (uses_u32) [[unlikely]]
    {
        if (const auto n = bitmask_size(dc); cells32.size() < n)
            cells32 = Array<u32>{DirectInit, n, empty_run<u32>};
        const bool built = build_impl(cells32, runs32, scratch_runs);
        fm_assert(built);
        build_u32_next = entry_data.size() > max_entries<u16> || runs32.size() > max_run_entries<u16>;
    }
}

template<typename T> bool ObjectGrid::build_impl(Array<T>& cells, Array<T>& runs, Array<u32>& scratch_runs)
{
    const auto dc = params.div_count, n = (u32)entry_ranges.size();
    if (n > max_entries<T>) [[unlikely]]
        return false;
    arrayClear(scratch_runs);

    // While counting, a cell of 2 or more entries holds run_bit | count.
    // A cell's second entry adds 3 run entries, its count and both indices, and each later one adds 1.
    u32 total = 1;
    for (auto k = 0u; k < n; k++)
    {
        const auto& r = entry_ranges[k];
        auto added = 0u;
        for (auto j = (u32)r.j_lo; j <= (u32)r.j_hi; j++)
            for (auto i = (u32)r.i_lo; i <= (u32)r.i_hi; i++)
            {
                auto& x = cells[j*dc + i];
                if (!(x & run_bit<T>))
                {
                    x = (T)(run_bit<T> | 2u);
                    added += 3;
                    arrayAppend(scratch_runs, j*dc + i);
                }
                else if (x != empty_run<T>)
                {
                    x++;
                    added++;
                }
                else
                    x = (T)k;
            }
        if (total + added > max_run_entries<T>) [[unlikely]]
        {
            clear_cells(cells, k + 1);
            return false;
        }
        total += added;
    }

    u32 pos = 1;
    for (auto i : scratch_runs)
    {
        const auto count = (u32)(cells[i] & ~run_bit<T>);
        cells[i] = (T)(run_bit<T> | pos);
        pos += 1 + count;
    }
    fm_debug_assert(pos == total);
    arrayClear(runs);
    reserve_scratch(runs, total);
    arrayResize(runs, ValueInit, total);

    for (auto k = 0u; k < n; k++)
    {
        const auto& r = entry_ranges[k];
        for (auto j = (u32)r.j_lo; j <= (u32)r.j_hi; j++)
            for (auto i = (u32)r.i_lo; i <= (u32)r.i_hi; i++)
                if (const auto x = cells[j*dc + i]; x & run_bit<T>)
                {
                    auto* run = runs.data() + (x & ~run_bit<T>);
                    run[++run[0]] = (T)k;
                }
    }
    return true;
}

template struct Pool<ObjectGrid>;

} // namespace floormat::Grid::detail

namespace floormat::Grid::Object {

Grid::~Grid() noexcept = default;
Grid::Grid(detail::ObjectGrid* grid): grid{grid} {}

u32 Grid::get_cell_index_from_coord(intra_coord pos) const
{
    return detail::GridBase::pack_bit_index_from_coord(pos, grid->params.div_count);
}

template<typename T> cell_object_view<T> Grid::objects_at_cell(u32 index) const
{
    detail::check_current(grid);
    fm_debug3_assert(index < grid->params.div_count * grid->params.div_count);
    fm_debug3_assert(grid->uses_u32 == std::is_same_v<T, u32>);
    if constexpr (std::is_same_v<T, u32>)
        return grid->objects_at_cell(grid->cells32, grid->runs32, index);
    else
        return grid->objects_at_cell(grid->cells, grid->runs, index);
}

template cell_object_view<u16> Grid::objects_at_cell<u16>(u32) const;
template cell_object_view<u32> Grid::objects_at_cell<u32>(u32) const;

ArrayView<const collision_data> Grid::entry_data() const
{
    detail::check_current(grid);
    return grid->entry_data;
}

ArrayView<const u8> Grid::entry_chunks() const { return grid->entry_chunks; }
bool Grid::uses_u32() const { return grid->uses_u32; }

bool Grid::blocked_objects(const Search::pred& p, Array<bool>& entry_blocked) const
{
    detail::check_current(grid);
    const auto n = (u32)grid->entry_data.size();
    detail::reserve_scratch(entry_blocked, n);
    arrayResize(entry_blocked, NoInit, n);
    bool any = false;
    for (auto i = 0u; i < n; i++)
    {
        const bool blocked = grid->is_entry_blocked(i, p);
        entry_blocked[i] = blocked;
        any |= blocked;
    }
    return any;
}

bool Grid::blocked_objects(const Search::pred& p) const
{
    detail::check_current(grid);
    for (auto i = 0u; i < (u32)grid->entry_data.size(); i++)
        if (grid->is_entry_blocked(i, p))
            return true;
    return false;
}

template<typename T> bool Grid::is_blocked(u32 index, ArrayView<const bool> entry_blocked) const
{
    fm_debug3_assert(index < grid->params.div_count * grid->params.div_count);
    fm_debug3_assert(grid->uses_u32 == std::is_same_v<T, u32>);
    if constexpr (std::is_same_v<T, u32>)
        return grid->is_blocked(grid->cells32, grid->runs32, index, entry_blocked);
    else
        return grid->is_blocked(grid->cells, grid->runs, index, entry_blocked);
}

template bool Grid::is_blocked<u16>(u32, ArrayView<const bool>) const;
template bool Grid::is_blocked<u32>(u32, ArrayView<const bool>) const;

template<typename T> bool Grid::is_empty_at(u32 index, u32 count) const
{
    fm_debug3_assert(count == 1 || count == 2);
    fm_debug3_assert(index + count <= grid->params.div_count * grid->params.div_count);
    fm_debug3_assert(grid->uses_u32 == std::is_same_v<T, u32>);
    const T* c;
    if constexpr (std::is_same_v<T, u32>)
        c = grid->cells32.data() + index;
    else
        c = grid->cells.data() + index;
    if (count == 1)
        return *c == detail::empty_run<T>;
    else
    {
        using W = std::conditional_t<std::is_same_v<T, u32>, u64, u32>;
        W v;
        std::memcpy(&v, c, sizeof v);
        return v == ((W)detail::empty_run<T> << sizeof(T)*8 | detail::empty_run<T>);
    }
}

template bool Grid::is_empty_at<u16>(u32, u32) const;
template bool Grid::is_empty_at<u32>(u32, u32) const;

void Grid::mask_pass_bits(ArrayView<const usize> pass_bits, ArrayView<usize> out,
                          ArrayView<const bool> entry_blocked) const
{
    detail::check_current(grid);
    fm_debug_assert(entry_blocked.size() == grid->entry_data.size());
    const auto n = detail::bitmask_size(grid->params.div_count) / (sizeof(usize)*8);
    fm_debug_assert(pass_bits.size() == n && out.size() == n);
    const auto& g = *grid;
    if (g.uses_u32)
        g.mask_pass_bits(g.cells32, g.runs32, pass_bits.data(), out.data(), entry_blocked);
    else
        g.mask_pass_bits(g.cells, g.runs, pass_bits.data(), out.data(), entry_blocked);
}

u32 Grid::div_count() const { return grid->params.div_count; }
u64 Grid::built_pass_gen() const { return grid->built_pass_gen; }
bool Grid::is_marked_stale() const { return grid->is_marked_stale(); }
bool Grid::is_stale() const
{
    return grid->is_marked_stale() || !grid->is_current() || grid->objects_maybe_dirty();
}
bool Grid::is_stale(u64 pass_gen) const { return pass_gen != grid->built_pass_gen || is_stale(); }
void Grid::mark_stale() { grid->mark_stale(); }
void Grid::build_if_stale() { grid->build_if_stale(); }

Pool::Pool(Pass::Pool& pass_pool): pool{new detail::ObjectPool{pass_pool}} {}
Pool::~Pool() noexcept { delete pool; }

Grid Pool::operator[](chunk& c)
{
    return Grid{detail::pool_subscript<detail::ObjectGrid>(pool, c)};
}

void Pool::build_if_stale_all()
{
    for (auto* g : pool->grids_in_use)
        static_cast<detail::ObjectGrid*>(g)->build_if_stale();
}

const Params& Pool::params() const { return pool->params; }
Pass::Pool& Pool::pass_pool() const { return pool->pass_pool; }
u32 Pool::pooled_count() const { return pool->freelist.size(); }

} // namespace floormat::Grid::Object
