#include "grid.hpp"
#include "chunk.hpp"
#include "world.hpp"
#include "compat/function2.hpp"
#include "compat/spinlock.hpp"
#include <bit>

namespace floormat::detail::grid {

namespace {
Spinlock g_build_seq_lock;
uint64_t g_build_seq = 0;
} // namespace

uint64_t GridBase::next_build_no()
{
    Locker<Spinlock> guard{g_build_seq_lock};
    return ++g_build_seq;
}

GridBase::GridBase(chunk& ch):
    c{&ch}, w{&ch.world()}, coord{ch.coord()}
{
    versions.fill((uint32_t)-1);
}

bool GridBase::is_stale() const
{
    return versions[8] == (uint32_t)-1;
}

void GridBase::mark_stale()
{
    versions[8] = (uint32_t)-1;
}

void GridBase::reset_base_for_reuse(chunk& ch)
{
    c = &ch;
    w = &ch.world();
    coord = ch.coord();
    neighbors = {};
    versions.fill((uint32_t)-1);
}

void GridBase::maybe_mark_stale_impl(fu2::function_view<chunk*(chunk_coords_) const> const& at_chunk)
{
    auto* current = at_chunk(coord);
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
        auto* nb = at_chunk(coord + world::neighbor_offsets[i]);

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

void GridBase::maybe_mark_stale()
{
    maybe_mark_stale_impl([this](chunk_coords_ ch) { return w->at(ch); });
}

uint32_t GridBase::pack_bit_index(uint32_t i, uint32_t j, uint32_t div_count)
{
    return j * div_count + i;
}

uint32_t GridBase::pack_bit_index_from_coord(local_coords local, Vector2b offset, uint32_t div_size, uint32_t div_count)
{
    constexpr auto half_tile = tile_size_xy/2;
    Vector2i posʹ;
    posʹ += Vector2i(local) * tile_size_xy;
    posʹ += Vector2i(offset);
    posʹ += Vector2i(half_tile);
    fm_debug2_assert(posʹ >= Vector2i{0});
    Vector2ui pos{NoInit}; (void)pos;
    if constexpr (std::has_single_bit(uint32_t{chunk_size_xy}))
        pos = Vector2ui(posʹ) >> (uint32_t)std::countr_zero(div_size);
    else
        pos = Vector2ui(posʹ) / div_size;
    auto idx = pack_bit_index(pos.x(), pos.y(), div_count);
    fm_assert(idx < div_count*div_count);
    return idx;
}

Range2D GridBase::coord_range_from_div(uint32_t x, uint32_t y, uint32_t div_size, uint32_t bbox_size)
{
    constexpr auto half_tile = Vector2i{tile_size_xy/2};
    const auto bbox = Vector2(bbox_size);
    const auto half_bbox = bbox*.5f;
    auto pos = Vector2i{(int32_t)x, (int32_t)y};
    pos *= Vector2i{(int32_t)div_size};
    pos += Vector2i(div_size / 2);
    pos -= half_tile;
    fm_debug_assert(pos >= -half_tile);
    fm_debug_assert(pos < Vector2i((int32_t)chunk_size_xy) - half_tile);
    auto posʹ = Vector2(pos);
    auto min = posʹ - half_bbox;
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
