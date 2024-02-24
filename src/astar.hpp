#pragma once
#include "path-search.hpp"
#include "point.hpp"
#include <bitset>
#include <Corrade/Containers/Array.h>

namespace floormat::detail_astar {

struct cache;
struct chunk_cache;

struct cache
{
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

} // namespace floormat::detail_astar

namespace floormat {

class astar
{
public:
    struct visited
    {
        uint32_t dist = (uint32_t)-1;
        uint32_t prev = (uint32_t)-1;
        point pt;
    };

    using pred = path_search::pred;
    template<typename T> using bbox = path_search::bbox<T>;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(astar);

    astar();
    void reserve(size_t capacity);
    void clear();

    // todo add simple bresenham short-circuit
    path_search_result Dijkstra(world& w, point from, point to,
                                object_id own_id, uint32_t max_dist, Vector2ub own_size,
                                int debug = 0, const pred& p = path_search::never_continue());

private:
    static constexpr auto initial_capacity = TILE_COUNT * 16 * detail_astar::div_factor*detail_astar::div_factor;

    struct chunk_cache;

    void add_to_heap(uint32_t id);
    uint32_t pop_from_heap();

    struct detail_astar::cache cache;
    Array<visited> nodes;
    Array<uint32_t> Q;
};

} // namespace floormat
