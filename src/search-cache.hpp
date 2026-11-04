#pragma once
#include "compat/defs.hpp"
#include "grid-pass.hpp"
#include <cr/Array.h>
#include <mg/Vector2.h>

namespace floormat {
class world;
struct point;
struct local_coords;
} // namespace floormat

namespace floormat::Search {

struct cache
{
    static constexpr usize max_size_bytes = usize{4} << 30;

    Vector2ui size;
    Vector2i start{(int)((1u << 31) - 1)};
    u32 div_size_ = 0;
    u32 div_count_ = 0;
    // A node's index and dist, both -1 once clear_chunk() ran for the cell's chunk in this search.
    // TODO Windows commits the whole window, 8 bytes a cell, though a search writes only the
    // chunks it clears. Reserve it and commit each chunk's slice in clear_chunk().
    struct cell { u32 index, dist; };
    Array<cell> cells;

    cache();
    ~cache() noexcept;
    fm_DISABLE_COPY(cache);

    u32 get_chunk_index(Vector2i chunk) const;
    static u32 get_chunk_index(Vector2i start, Vector2ui size, Vector2i coord);
    u32 get_tile_index(intra_coord pos) const;
    static Vector2ui get_size_to_allocate(u32 max_dist);

    [[nodiscard]] u32 allocate(point from, u32 max_dist, u32 div_size);
    void add_index(u32 chunk_index, u32 tile_index, u32 index, u32 dist);
    u32 lookup_index(u32 chunk_index, u32 tile_index);
    cell& at(u32 chunk_index, u32 tile_index);
    // Drops the chunk's nodes, so it runs once per chunk and search, before the chunk's first lookup.
    void clear_chunk(u32 chunk_index);

    bool is_passable_for_bbox(world& w, Grid::Pass::Pool& pool, point pt, const pred& p);
};

} // namespace floormat::Search
