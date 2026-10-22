#include "grid.hpp"
#include "intra-coord.inl"
#include "chunk.hpp"
#include "world.hpp"
#include "compat/function2.hpp"
#include "compat/spinlock.hpp"
#include <bit>
#include <cr/GrowableArray.h>

namespace floormat::detail::grid {

namespace {
constinit Spinlock g_build_seq_lock;
uint64_t g_build_seq = 0;

constinit Spinlock g_slot_lock;
constinit Array<uint32_t> g_free_slots;
uint32_t g_slot_count = 0;

uint32_t take_slot_index()
{
    Locker<Spinlock> guard{g_slot_lock};
    if (!g_free_slots.isEmpty())
    {
        const auto i = g_free_slots.back();
        arrayRemoveSuffix(g_free_slots);
        return i;
    }
    return g_slot_count++;
}

void put_slot_index(uint32_t i)
{
    Locker<Spinlock> guard{g_slot_lock};
    arrayAppend(g_free_slots, i);
}
} // namespace

uint64_t GridBase::next_build_no()
{
    Locker<Spinlock> guard{g_build_seq_lock};
    return ++g_build_seq;
}

GridBase::GridBase(chunk& ch):
    c{&ch}, coord{ch.coord()}
{
    versions.fill((uint64_t)-1);
}

bool GridBase::is_stale() const
{
    return versions[8] == (uint64_t)-1;
}

void GridBase::mark_stale()
{
    versions[8] = (uint64_t)-1;
}

void GridBase::reset_base_for_reuse(chunk& ch)
{
    c = &ch;
    coord = ch.coord();
    neighbors = {};
    versions.fill((uint64_t)-1);
}

bool GridBase::versions_match() const
{
    if (versions[8] != c->pass_gen())
        return false;

    auto& w = c->world();
    for (auto i = 0u; i < 8; i++)
    {
        auto* nb = w.at(coord + world::neighbor_offsets[i]);
        auto nb_ver = nb ? nb->pass_gen() : (uint64_t)-1;
        if (nb_ver != versions[i])
            return false;
    }
    return true;
}

void GridBase::maybe_mark_stale()
{
    fm_debug3_assert(c && c->world().at(coord) == c);

    if (!is_stale() && !versions_match())
        mark_stale();
}

uint32_t GridBase::pack_bit_index(uint32_t i, uint32_t j, uint32_t div_count)
{
    return j * div_count + i;
}

uint32_t GridBase::pack_bit_index_from_coord(intra_coord posʹ, uint32_t div_count)
{
    const auto pos = Vector2ui(Vector2i(posʹ)) >> (uint32_t)(std::countr_zero(chunk_size_xy) - std::countr_zero(div_count)); // xxx todo assumes power of two chunk size
    // per-axis: an out-of-range x with a small y still packs below div_count²
    fm_debug3_assert(pos.x() < div_count && pos.y() < div_count);
    return pack_bit_index(pos.x(), pos.y(), div_count);
}

Range2D GridBase::coord_range_from_div(uint32_t x, uint32_t y, uint32_t div_size, uint32_t bbox_size)
{
    const auto bbox = Vector2(bbox_size);
    const auto pos = intra_coord{Vector2i{(int32_t)x, (int32_t)y} * (int32_t)div_size + Vector2i{(int32_t)(div_size / 2)}};
    auto min = Vector2(pos.center_shifted()) - Vector2((float)(bbox_size / 2));
    auto max = min + bbox;
    return { min, max };
}

std::pair<uint32_t, uint8_t> GridBase::byte_and_mask(uint32_t bit_index)
{
    return { bit_index >> 3, uint8_t(1u << (bit_index & 7)) };
}

GridBase* free_list::take()
{
    auto* p = list;
    if (p)
    {
        list = p->next;
        p->next = nullptr;
    }
    return p;
}

void free_list::put(GridBase* p)
{
    p->next = list;
    list = p;
}

bool free_list::is_empty() const { return !list; }

uint32_t free_list::size() const
{
    uint32_t n = 0;
    for (auto* p = list; p; p = p->next)
        n++;
    return n;
}

grid_slots::grid_slots() noexcept = default;

grid_slots::~grid_slots() noexcept
{
    for (auto* g : slots)
        if (g)
            g->pool->release(g);
}

PoolBase::PoolBase(): slot_index{take_slot_index()} {}

PoolBase::~PoolBase() noexcept
{
    fm_assert(live.isEmpty());
    put_slot_index(slot_index);
}

void PoolBase::attach(chunk& c, GridBase* g)
{
    fm_debug_assert(g->c == &c && g->live_index == (uint32_t)-1);
    g->pool = this;
    g->live_index = (uint32_t)live.size();
    arrayAppend(live, g);
    if (!c._grid_slots)
        c._grid_slots.emplace();
    auto& s = c._grid_slots->slots;
    if (slot_index >= s.size())
        arrayResize(s, ValueInit, slot_index + 1);
    fm_debug_assert(!s[slot_index]);
    s[slot_index] = g;
}

void PoolBase::release(GridBase* g) noexcept
{
    fm_debug_assert(g->pool == this && g->live_index < live.size() && live[g->live_index] == g);
    auto* last = live.back();
    live[g->live_index] = last;
    last->live_index = g->live_index;
    arrayRemoveSuffix(live);
    g->live_index = (uint32_t)-1;
    g->c = nullptr;
    freelist.put(g);
}

void PoolBase::detach_all() noexcept
{
    for (auto* g : live)
    {
        auto& s = g->c->_grid_slots->slots;
        fm_debug_assert(s[slot_index] == g);
        s[slot_index] = nullptr;
        g->c = nullptr;
    }
}

bool PoolBase::update_checked_pass_gen(const world& w) noexcept
{
    // Every stamp in versions[] came from world::next_pass_gen(), and so did every change that
    // dirtied a chunk. While the counter hasn't moved, the walk would find nothing.
    const auto gen = w.pass_gen();
    if (gen == checked_pass_gen)
        return false;
    checked_pass_gen = gen;
    return true;
}

void cascade_mark_neighbors_stale(
    chunk_coords_ coord,
    fu2::function_view<GridBase*(chunk_coords_)> find)
{
    for (auto off : world::neighbor_offsets)
        if (auto* g = find(coord + off))
            g->mark_stale();
}

bool BitView::read(uint32_t i) const
{
    auto [byte, mask] = GridBase::byte_and_mask(i);
    return data[byte] & mask;
}

void BitView::set(uint32_t i)
{
    auto [byte, mask] = GridBase::byte_and_mask(i);
    data[byte] |= mask;
}

void BitView::reset(uint32_t i)
{
    auto [byte, mask] = GridBase::byte_and_mask(i);
    data[byte] &= uint8_t(~mask);
}

void BitView::write(uint32_t i, bool value)
{
    auto [byte_idx, mask] = GridBase::byte_and_mask(i);
    auto& byte = data[byte_idx];
    byte = uint8_t((byte & ~mask) | (uint8_t(value) << (i & 7)));
}

} // namespace floormat::detail::grid
