#pragma once
#include "compat/safe-ptr.hpp"
#include "search-constants.hpp"
#include "search-pred.hpp"
#include "object-id.hpp"
#include <Corrade/Containers/Array.h>

namespace floormat::Search { struct cache; }

namespace floormat {

class world;
struct point;
struct path_search_result;

class astar
{
public:
    struct visited;
    using pred = Search::pred;

    astar();
    ~astar() noexcept;
    void reserve(size_t capacity);
    void clear();

    // todo add simple bresenham short-circuit
    path_search_result Dijkstra(world& w, point from, point to,
                                object_id own_id, uint32_t max_dist, Vector2ub own_size,
                                int debug = 0, const pred& p = Search::never_continue());

private:
    static constexpr auto initial_capacity = TILE_COUNT * 16 * Search::div_factor*Search::div_factor;

    void add_to_heap(uint32_t id);
    uint32_t pop_from_heap();

    safe_ptr<struct Search::cache> _cache;
    Array<visited> nodes;
    Array<uint32_t> Q;
    Array<point> temp_nodes;
};

} // namespace floormat
