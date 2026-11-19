#include "search-cache.hpp"
#include "search.hpp"
#include "grid-registry.hpp"
#include "point.inl"
#include "intra-coord.inl"
#include "world.hpp"
#include "tile-defs.hpp"
#include "compat/function-view.hpp"
#include "search-octile.hpp"
#include "compat/fill.hpp"
#include <bit>

namespace floormat::Search {

cache::cache() = default;

cache::~cache() noexcept = default;

namespace {

struct chunk_rect { Vector2i start; Vector2ui size; };

i64 chunk_of(i64 px) { return px >= 0 ? px / chunk_size_xy : -((-px + chunk_size_xy - 1) / chunk_size_xy); }

// A node n has g + h < max_dist, and g, h are each at most 1 below the octile distance, so
// |n - from| + |n - to| < max_dist + 2 per axis.
chunk_rect window(point from, point to, u32 max_dist)
{
    const auto d = to - from;
    const auto M = (i64)max_dist + 2;
    const auto f = Vector2i(from.chunk());
    if (Math::max(Math::abs(d.x()), Math::abs(d.y())) >= M)
        return { f - Vector2i{1}, Vector2ui{3} };

    const auto base = Vector2i(intra_coord{from});
    chunk_rect r;
    for (auto i = 0u; i < 2; i++)
    {
        const i64 s = 2*(i64)base[i] + d[i];
        // Each side gets one more chunk, since A* reads cells less than a chunk beyond the nodes, `from` and `to`.
        const auto lo = chunk_of(((s - M) >> 1) + 1) - 1, hi = chunk_of(((s + M + 1) >> 1) - 1) + 1;
        r.start[i] = f[i] + (int)lo;
        r.size[i] = (u32)(hi - lo + 1);
    }
    return r;
}

// The nodes span up to max_dist + 2 positions per axis.
u32 max_window_side(u32 max_dist) { return (u32)(((u64)max_dist + 1) / chunk_size_xy + 4); }

// Nodes lie in the octagon of octile radius R around the from/to midpoint, of area 4R²·axial/diag and
// perimeter under 8R. By Pick's theorem it holds at most area + half the perimeter + 1 lattice points.
double node_bound(u32 max_dist, u32 div_size)
{
    const auto oct = octile{div_size};
    const double R = max_dist/2. + div_size, d = div_size;
    // The start's seed search adds at most one node per position of its window, plus its seeds. The goal's
    // seed search builds at most two paths, its path from `from` and the one make_result() takes.
    constexpr double W = 2*seed_search_radius + 1;
    const auto seed_side = (double)(2*(seed_search_radius + div_size - 1)/div_size + 1);
    const double seed_nodes = 3*W*W + seed_side*seed_side + 16;
    return Math::floor(4*R*R*oct.axial/(oct.diag*d*d) + 8*R/d) + 2 + seed_nodes;
}

u64 max_bytes(u32 max_dist, u32 div_size)
{
    const auto side = (u64)max_window_side(max_dist), cells_per_chunk = (u64)(chunk_size_xy/div_size)*(chunk_size_xy/div_size);
    const auto nodes = node_bound(max_dist, div_size);
    if (nodes >= (double)(u64{1} << 40))
        return (u64)-1;
    return side*side*cells_per_chunk*sizeof(cache::cell) + (u64)nodes*cache::node_size;
}

} // namespace

Vector2ui cache::get_size_to_allocate(point from, point to, u32 max_dist)
{
    return window(from, to, max_dist).size;
}

u32 cache::max_node_count(u32 max_dist, u32 div_size)
{
    const auto n = node_bound(max_dist, div_size);
    fm_assert(n < (double)(u64{1} << 32));
    return (u32)n;
}

u32 cache::max_dist_limit(u32 div_size)
{
    fm_assert(div_size > 0 && chunk_size_xy % div_size == 0);
    u64 lo = 0, hi = (u32)-1;
    while (lo < hi)
    {
        const auto mid = lo + (hi - lo + 1)/2;
        if (max_bytes((u32)mid, div_size) <= max_size_bytes)
            lo = mid;
        else
            hi = mid - 1;
    }
    return (u32)lo;
}

u32 cache::allocate(point from, point to, u32 max_dist, u32 div_size)
{
    fm_assert(from.coord().z() == 0);
    fm_assert(div_size > 0);
    fm_assert(chunk_size_xy % div_size == 0);
    div_size_ = div_size;
    div_count_ = chunk_size_xy / div_size;

    auto need = max_bytes(max_dist, div_size);
    if (need > max_size_bytes) [[unlikely]]
    {
        max_dist = max_dist_limit(div_size);
        need = max_bytes(max_dist, div_size);
    }

    const auto r = window(from, to, max_dist);
    fm_assert((r.size <= Vector2ui{max_window_side(max_dist)}).all());
    start = r.start;
    size = r.size;
    const auto total_cells = (usize)size.product() * div_count_ * div_count_;
    const auto cells_bytes = total_cells * sizeof(cell);

    if (memory.size() < need)
    {
        memory.reserve(std::bit_ceil(need));
        nodes_committed = 0;
        cells_committed = memory.size();
    }
    const auto cells_at = memory.size() - cells_bytes;
    if (cells_at < cells_committed)
    {
        memory.commit(cells_at, cells_committed - cells_at);
        cells_committed = cells_at & ~(virtual_memory::page_size() - 1);
    }
    cells = { reinterpret_cast<cell*>(memory.data() + cells_at), total_cells };

    return max_dist;
}

ArrayView<char> cache::commit_nodes(usize bytes)
{
    const auto limit = (usize)(reinterpret_cast<char*>(cells.data()) - memory.data());
    bytes = Math::min(bytes, limit);
    if (bytes > nodes_committed)
    {
        // allocate() already committed the bytes at or above cells_committed, for cells.
        if (const auto end = Math::min(bytes, cells_committed); end > nodes_committed)
            memory.commit(nodes_committed, end - nodes_committed);
        const auto page = virtual_memory::page_size();
        nodes_committed = Math::min((bytes + page - 1) & ~(page - 1), memory.size());
    }
    return { memory.data(), Math::min(nodes_committed, limit) };
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
    // xxx todo assumes power of two chunk size
    const auto pos = Vector2ui(Vector2i(posʹ)) >> (u32)std::countr_zero(div_size_);
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
