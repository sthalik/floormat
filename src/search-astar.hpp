#pragma once
#include "compat/safe-ptr.hpp"
#include "search-constants.hpp"
#include "search-pred.hpp"
#include "object-id.hpp"
#include <cr/Array.h>

namespace floormat::Search { struct cache; }
namespace floormat::Grid::Pass { class PoolRegistry; }

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
    template<int Debug = 0>
    path_search_result Dijkstra(world& w, point from, point to,
                                uint32_t max_dist, Vector2ui own_size,
                                const pred& p,
                                const heuristic& h = Search::octile_distance());

private:
    static constexpr auto initial_capacity = TILE_COUNT * 32 * Search::div_factor*Search::div_factor;

    safe_ptr<struct Search::cache> _cache;
    safe_ptr<Grid::Pass::PoolRegistry> _registry;
    Array<visited> nodes;
    Array<frontier> Q;
    Array<point> temp_nodes;
};

} // namespace floormat
