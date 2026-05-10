#include "search-cache.hpp"
#include "search.hpp"
#include "grid-pass-pool.hpp"
#include "point.inl"
#include "world.hpp"
#include "local-coords.hpp"
#include "tile-defs.hpp"
#include "compat/function2.hpp"
#include <bit>

namespace floormat::Search {

namespace {
constexpr auto chunk_size_xy = (uint32_t)tile_size_xy * (uint32_t)TILE_MAX_DIM;
} // namespace

cache::cache(uint32_t div_size)
    : div_size_{div_size},
      div_count_{chunk_size_xy / div_size}
{
    fm_assert(div_size > 0);
    fm_assert(chunk_size_xy % div_size == 0);
}

cache::~cache() noexcept = default;

Vector2ui cache::get_size_to_allocate(uint32_t max_dist)
{
    constexpr auto chunk_size = Vector2ui{chunk_size_xy};
    constexpr auto rounding   = chunk_size - Vector2ui(1);
    auto nchunks = (Vector2ui(max_dist) + rounding) / chunk_size;
    return nchunks + Vector2ui(3);
}

void cache::allocate(point from, uint32_t max_dist)
{
    fm_assert(from.coord().z() == 0);
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
        exists.resetAll();
    }
}

size_t cache::get_chunk_index(Vector2i start, Vector2ui size, Vector2i coord)
{
    auto off = Vector2ui(coord - start);
    fm_assert(off < size);
    auto index = off.y() * size.x() + off.x();
    fm_debug_assert(index < size.product());
    return index;
}

size_t cache::get_chunk_index(Vector2i chunk) const { return get_chunk_index(start, size, chunk); }

size_t cache::get_tile_index(local_coords local, Vector2b offset_) const
{
    constexpr auto half_tile = (int32_t)tile_size_xy / 2;
    Vector2i posʹ;
    posʹ += Vector2i(local) * (int32_t)tile_size_xy;
    posʹ += Vector2i(offset_);
    posʹ += Vector2i(half_tile);
    fm_debug_assert(posʹ >= Vector2i{0});
    Vector2ui pos;
    if constexpr (std::has_single_bit(uint32_t{chunk_size_xy}))
        pos = Vector2ui(posʹ) >> (uint32_t)std::countr_zero(div_size_);
    else
        pos = Vector2ui(posʹ) / div_size_;
    auto idx = (size_t)pos.y() * div_count_ + (size_t)pos.x();
    fm_debug_assert(idx < (size_t)div_count_ * div_count_);
    return idx;
}

void cache::add_index(size_t chunk_index, size_t tile_index, uint32_t index)
{
    fm_debug_assert(index != (uint32_t)-1);
    auto cells_per_chunk = (size_t)div_count_ * div_count_;
    auto flat = chunk_index * cells_per_chunk + tile_index;
    fm_debug_assert(!exists[flat]);
    exists.set(flat);
    indexes[flat] = index;
}

void cache::add_index(point pt, uint32_t index)
{
    fm_assert(pt.coord().z() == 0);
    auto ch = get_chunk_index(Vector2i(pt.chunk()));
    auto tile = get_tile_index(pt.local(), pt.offset());
    add_index(ch, tile, index);
}

uint32_t cache::lookup_index(size_t chunk_index, size_t tile_index)
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
    if (auto* c = w.chunk_at_memo(pt.chunk3()))
    {
        auto grid = pool[*c];
        grid.build_if_stale(p);
        return grid.bit(grid.get_bitmask_index_from_coord(pt.local(), pt.offset()));
    }

    auto nbs = w.neighbors(pt.chunk3());
    auto half = (float)pool.params().bbox_size * .5f;
    auto center = Vector2(Vector2i(pt.local()) * (int32_t)tile_size_xy + Vector2i(pt.offset()));
    return Search::is_passable_(nullptr, nbs, center - Vector2{half}, center + Vector2{half}, p);
}

bool cache::is_passable_between_diag(world& w, Grid::Pass::Pool& pool, point a, point b, const pred& p)
{
    if (!is_passable_for_bbox(w, pool, b, p))
        return false;
    Vector2i vec = b - a;
    // off-axis cells; coverage relies on the +div_size inflation in grid-pass.cpp
    if (!is_passable_for_bbox(w, pool, a + Vector2i{vec.x(), 0}, p))
        return false;
    if (!is_passable_for_bbox(w, pool, a + Vector2i{0, vec.y()}, p))
        return false;
    return true;
}

} // namespace floormat::Search
