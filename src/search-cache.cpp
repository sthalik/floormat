#include "search-cache.hpp"
#include "search.hpp"
#include "grid-registry.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "world.hpp"
#include "tile-defs.hpp"
#include "compat/function2.hpp"
#include "compat/fill.hpp"
#include <bit>

namespace floormat::Search {

cache::cache() = default;

cache::~cache() noexcept = default;

Vector2ui cache::get_size_to_allocate(u32 max_dist)
{
    constexpr auto rounding = chunk_size<Vector2ui> - Vector2ui(1);
    auto nchunks = (Vector2ui(max_dist) + rounding) / chunk_size<Vector2ui>;
    // A node lies within nchunks of from's chunk. Its 3x3 window and A*'s seeds reach
    // one chunk further.
    return nchunks + Vector2ui(1);
}

u32 cache::allocate(point from, u32 max_dist, u32 div_size)
{
    fm_assert(from.coord().z() == 0);
    fm_assert(div_size > 0);
    fm_assert(chunk_size_xy % div_size == 0);
    div_size_ = div_size;
    div_count_ = chunk_size_xy / div_size;
    auto cells_per_chunk = (usize)div_count_ * div_count_;
    const auto chunk_bytes = cells_per_chunk * sizeof(cell);
    const auto max_chunk_count = max_size_bytes / chunk_bytes;

    const auto side = 2 * (((usize)max_dist + chunk_size_xy - 1) / chunk_size_xy) + 3;
    if (side * side > max_chunk_count) [[unlikely]]
    {
        auto max_side = (usize)Math::sqrt((double)max_chunk_count);
        max_side -= max_side % 2 == 0;
        const auto clamped = (u32)(max_side - 3) / 2 * chunk_size_xy;
        constexpr double MiB = 1 << 20;
        fm_warn_once("search cache: max_dist %u at div %u needs %.1f MiB, over %.1f MiB; clamped to %u, %.1f MiB",
                     max_dist, div_size, (double)side * (double)side * (double)chunk_bytes / MiB,
                     (double)max_size_bytes / MiB, clamped,
                     (double)(max_side * max_side * chunk_bytes) / MiB);
        max_dist = clamped;
    }

    auto off = get_size_to_allocate(max_dist);
    start = Vector2i(from.chunk()) - Vector2i(off);
    size = off * 2u + Vector2ui(1);
    auto len = (usize)size.product();
    auto total_cells = len * cells_per_chunk;

    if (total_cells > cells.size())
        cells = Array<cell>{NoInit, total_cells};

    return max_dist;
}

u32 cache::get_chunk_index(Vector2i start, Vector2ui size, Vector2i coord)
{
    auto off = Vector2ui(coord - start);
    fm_debug3_assert(off < size);
    auto index = off.y() * size.x() + off.x();
    fm_debug3_assert(index < size.product());
    return index;
}

u32 cache::get_chunk_index(Vector2i chunk) const { return get_chunk_index(start, size, chunk); }

u32 cache::get_tile_index(intra_coord posʹ) const
{
    const auto pos = Vector2ui(Vector2i(posʹ)) >> (u32)std::countr_zero(div_size_); // xxx todo assumes power of two chunk size
    auto idx = pos.y() * div_count_ + pos.x();
    fm_debug3_assert(idx < div_count_ * div_count_);
    return idx;
}

void cache::add_index(u32 chunk_index, u32 tile_index, u32 index, u32 dist)
{
    fm_debug3_assert(index != (u32)-1);
    auto& c = at(chunk_index, tile_index);
    fm_debug3_assert(c.index == (u32)-1);
    c = { index, dist };
}

u32 cache::lookup_index(u32 chunk_index, u32 tile_index)
{
    return at(chunk_index, tile_index).index;
}

auto cache::at(u32 chunk_index, u32 tile_index) -> cell&
{
    auto cells_per_chunk = (usize)div_count_ * div_count_;
    const auto i = chunk_index * cells_per_chunk + tile_index;
    fm_debug3_assert(i < cells.size());
    // Not operator[]: its bounds check stays in release builds, and do_dir() calls this per neighbour.
    return cells.data()[i];
}

void cache::clear_chunk(u32 chunk_index)
{
    auto cells_per_chunk = (usize)div_count_ * div_count_;
    fill(cells.sliceSize(chunk_index * cells_per_chunk, cells_per_chunk), cell{(u32)-1, (u32)-1});
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
