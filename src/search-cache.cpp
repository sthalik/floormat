#include "search-cache.hpp"
#include "search.hpp"
#include "grid-pool-registry.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "world.hpp"
#include "tile-defs.hpp"
#include "compat/function2.hpp"
#include <bit>
#include <cr/BitArrayView.h>

namespace floormat::Search {

cache::cache() = default;

cache::~cache() noexcept = default;

Vector2ui cache::get_size_to_allocate(uint32_t max_dist)
{
    constexpr auto rounding = chunk_size<Vector2ui> - Vector2ui(1);
    auto nchunks = (Vector2ui(max_dist) + rounding) / chunk_size<Vector2ui>;
    // A node lies within nchunks of from's chunk. Its 3x3 window and A*'s seeds reach
    // one chunk further.
    return nchunks + Vector2ui(1);
}

void cache::allocate(point from, uint32_t max_dist, uint32_t div_size)
{
    fm_assert(from.coord().z() == 0);
    fm_assert(div_size > 0);
    fm_assert(chunk_size_xy % div_size == 0);
    div_size_ = div_size;
    div_count_ = chunk_size_xy / div_size;
    auto off = get_size_to_allocate(max_dist);
    start = Vector2i(from.chunk()) - Vector2i(off);
    size = off * 2u + Vector2ui(1);
    auto len = (size_t)size.product();
    auto cells_per_chunk = (size_t)div_count_ * div_count_;
    auto total_cells = len * cells_per_chunk;

    if (total_cells > exists.size())
    {
        indexes = Array<uint32_t>{NoInit, total_cells};
        exists = BitArray{ValueInit, total_cells};
    }
    else
    {
        exists.prefix(total_cells).resetAll();
    }
}

uint32_t cache::get_chunk_index(Vector2i start, Vector2ui size, Vector2i coord)
{
    auto off = Vector2ui(coord - start);
    fm_debug3_assert(off < size);
    auto index = off.y() * size.x() + off.x();
    fm_debug3_assert(index < size.product());
    return index;
}

uint32_t cache::get_chunk_index(Vector2i chunk) const { return get_chunk_index(start, size, chunk); }

uint32_t cache::get_tile_index(intra_coord posʹ) const
{
    const auto pos = Vector2ui(Vector2i(posʹ)) >> (uint32_t)std::countr_zero(div_size_); // xxx todo assumes power of two chunk size
    auto idx = pos.y() * div_count_ + pos.x();
    fm_debug3_assert(idx < div_count_ * div_count_);
    return idx;
}

void cache::add_index(uint32_t chunk_index, uint32_t tile_index, uint32_t index)
{
    fm_debug3_assert(index != (uint32_t)-1);
    auto cells_per_chunk = (size_t)div_count_ * div_count_;
    auto flat = chunk_index * cells_per_chunk + tile_index;
    fm_debug3_assert(!exists[flat]);
    exists.set(flat);
    indexes[flat] = index;
}

uint32_t cache::lookup_index(uint32_t chunk_index, uint32_t tile_index)
{
    auto cells_per_chunk = (size_t)div_count_ * div_count_;
    auto flat = chunk_index * cells_per_chunk + tile_index;
    if (exists[flat])
        return indexes[flat];
    else
        return (uint32_t)-1;
}

bool cache::is_passable_for_bbox(world& w, Grid::Pass::Pool& pool, point pt, const pred& p)
{
    if (auto* c = w.at(pt.chunk3()))
    {
        auto grid = pool[*c];
        grid.build_if_stale();
        return grid.bit(grid.get_bitmask_index_from_coord(intra_coord{pt}));
    }

    auto nbs = w.neighbors(pt.chunk3());
    const auto size = pool.params().bbox_size;
    const auto min = Vector2(intra_coord{pt}.center_shifted()) - Vector2{(float)(size / 2)};
    return Search::is_passable_(nullptr, nbs, min, min + Vector2{(float)size}, p);
}

} // namespace floormat::Search
