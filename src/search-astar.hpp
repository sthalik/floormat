#pragma once
#include "compat/safe-ptr.hpp"
#include "search-constants.hpp"
#include "search-pred.hpp"
#include "object-id.hpp"
#include "search-pool.hpp"
#include <cr/Array.h>

namespace floormat::Search { struct cache; }

namespace floormat {

class world;
struct point;
struct path_search_result;

class astar
{
public:
    struct visited;
    struct frontier;

    using pred = Search::pred;
    using heuristic = Search::heuristic;

    astar();
    ~astar() noexcept;
    void reserve(size_t capacity);
    void clear();

    // todo add simple bresenham short-circuit
    path_search_result Dijkstra(world& w, point from, point to,
                                object_id own_id, uint32_t max_dist, Vector2ui own_size,
                                int debug = 0,
                                const pred& p = Search::never_continue(),
                                const heuristic& h = Search::octile_distance());

private:
    static constexpr auto initial_capacity = TILE_COUNT * 16 * Search::div_factor*Search::div_factor;

    void add_to_heap(uint32_t id, uint32_t f_score, uint32_t g_score);
    frontier pop_from_heap();

    safe_ptr<struct Search::cache> _cache;
    Pass::PoolRegistry _pool_registry{(uint32_t)Search::div_size.x()};
    Array<visited> nodes;
    Array<frontier> Q;
    Array<point> temp_nodes;
};

} // namespace floormat
