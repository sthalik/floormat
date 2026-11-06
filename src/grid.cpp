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
u64 g_build_seq = 0;

constinit Spinlock g_slot_lock;
constinit Array<u32> g_free_slots;
u32 g_slot_count = 0;

u32 take_slot_index()
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

void put_slot_index(u32 i)
{
    Locker<Spinlock> guard{g_slot_lock};
    arrayReserve(g_free_slots, 16);
    arrayAppend(g_free_slots, i);
}
} // namespace

u64 GridBase::next_build_no()
{
    Locker<Spinlock> guard{g_build_seq_lock};
    return ++g_build_seq;
}

GridBase::GridBase(chunk& ch):
    c{&ch}, coord{ch.coord()}
{
}

bool GridBase::is_stale() const
{
    return built_pass_gen == 0;
}

void GridBase::mark_stale()
{
    built_pass_gen = 0;
}

void GridBase::reset_base_for_reuse(chunk& ch)
{
    c = &ch;
    coord = ch.coord();
    neighbors = {};
    built_pass_gen = 0;
}

bool GridBase::is_current() const
{
    // A neighbour emptied and then deleted by world::collect() goes unseen. Chunks normally
    // disappear only while laying out a map in the editor.
    if (c->pass_gen() > built_pass_gen)
        return false;

    auto& w = c->world();
    for (auto off : world::neighbor_offsets)
        if (auto* nb = w.at(coord + off); nb && nb->pass_gen() > built_pass_gen)
            return false;
    return true;
}

void GridBase::maybe_mark_stale()
{
    fm_debug3_assert(c && c->world().at(coord) == c);

    if (!is_stale() && !is_current())
        mark_stale();
}

u32 GridBase::pack_bit_index(u32 i, u32 j, u32 div_count)
{
    return j * div_count + i;
}

u32 GridBase::pack_bit_index_from_coord(intra_coord posʹ, u32 div_count)
{
    const auto pos = Vector2ui(Vector2i(posʹ)) >> (u32)(std::countr_zero(chunk_size_xy) - std::countr_zero(div_count)); // xxx todo assumes power of two chunk size
    // per-axis: an out-of-range x with a small y still packs below div_count²
    fm_debug3_assert(pos.max() < div_count);
    return pack_bit_index(pos.x(), pos.y(), div_count);
}

Range2D GridBase::coord_range_from_div(u32 x, u32 y, u32 div_size, u32 bbox_size)
{
    const auto bbox = Vector2(bbox_size);
    const auto pos = intra_coord{Vector2i{(i32)x, (i32)y} * (i32)div_size + Vector2i{(i32)(div_size / 2)}};
    const auto min = Vector2(pos.center_shifted()) - Vector2((float)(bbox_size / 2));
    return Range2D::fromSize(min, bbox);
}

std::pair<u32, u8> GridBase::byte_and_mask(u32 bit_index)
{
    return { bit_index >> 3, u8(1u << (bit_index & 7)) };
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

u32 free_list::size() const
{
    u32 n = 0;
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

PoolBase::PoolBase(): slot_index{take_slot_index()}
{
    arrayReserve(live, 16);
}

PoolBase::~PoolBase() noexcept
{
    fm_assert(live.isEmpty());
    put_slot_index(slot_index);
}

void PoolBase::attach(chunk& c, GridBase* g)
{
    fm_debug_assert(g->c == &c && g->live_index == (u32)-1);
    g->pool = this;
    g->live_index = (u32)live.size();
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
    g->live_index = (u32)-1;
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
    // is_current() fails only on a chunk stamp newer than the build, and every stamp comes from
    // world::next_pass_gen(). While the counter hasn't moved, the walk would find nothing.
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

bool BitView::read(u32 i) const
{
    auto [byte, mask] = GridBase::byte_and_mask(i);
    return data[byte] & mask;
}

void BitView::set(u32 i)
{
    auto [byte, mask] = GridBase::byte_and_mask(i);
    data[byte] |= mask;
}

void BitView::reset(u32 i)
{
    auto [byte, mask] = GridBase::byte_and_mask(i);
    data[byte] &= u8(~mask);
}

void BitView::write(u32 i, bool value)
{
    auto [byte_idx, mask] = GridBase::byte_and_mask(i);
    auto& byte = data[byte_idx];
    byte = u8((byte & ~mask) | (u8(value) << (i & 7)));
}

} // namespace floormat::detail::grid
