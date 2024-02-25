#pragma once
#include "compat/defs.hpp"
#include <array>
#include <Corrade/Containers/Array.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {
class world;
class chunk;
struct point;
struct chunk_coords_;
} // namespace floormat

namespace floormat::Search {

struct cache
{
    struct chunk_cache;

    Vector2ui size;
    Vector2i start{(int)((1u << 31) - 1)};
    Array<chunk_cache> array;

    cache();
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(cache);

    size_t get_chunk_index(Vector2i chunk) const;
    static size_t get_chunk_index(Vector2i start, Vector2ui size, Vector2i coord);
    static size_t get_tile_index(Vector2i pos, Vector2b offset);
    static Vector2ui get_size_to_allocate(uint32_t max_dist);

    void allocate(point from, uint32_t max_dist);
    void add_index(size_t chunk_index, size_t tile_index, uint32_t index);
    void add_index(point pt, uint32_t index);
    uint32_t lookup_index(size_t chunk_index, size_t tile_index);
    chunk* try_get_chunk(world& w, chunk_coords_ ch);

    std::array<chunk*, 8> get_neighbors(world& w, chunk_coords_ ch0);
};

} // namespace floormat::Search
