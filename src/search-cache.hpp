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
    static constexpr size_t max_size_bytes = size_t{4} << 30;

    Vector2ui size;
    Vector2i start{(int)((1u << 31) - 1)};
    uint32_t div_size_ = 0;
    uint32_t div_count_ = 0;
    // A node's index, or -1 once clear_chunk() ran for the cell's chunk in this search.
    Array<uint32_t> indexes;

    cache();
    ~cache() noexcept;
    fm_DISABLE_COPY(cache);

    uint32_t get_chunk_index(Vector2i chunk) const;
    static uint32_t get_chunk_index(Vector2i start, Vector2ui size, Vector2i coord);
    uint32_t get_tile_index(intra_coord pos) const;
    static Vector2ui get_size_to_allocate(uint32_t max_dist);

    [[nodiscard]] uint32_t allocate(point from, uint32_t max_dist, uint32_t div_size);
    void add_index(uint32_t chunk_index, uint32_t tile_index, uint32_t index);
    uint32_t lookup_index(uint32_t chunk_index, uint32_t tile_index);
    // Drops the chunk's nodes, so it runs once per chunk and search, before the chunk's first lookup.
    void clear_chunk(uint32_t chunk_index);

    bool is_passable_for_bbox(world& w, Grid::Pass::Pool& pool, point pt, const pred& p);
};

} // namespace floormat::Search
