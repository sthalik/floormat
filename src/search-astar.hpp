#pragma once
#include "compat/safe-ptr.hpp"
#include "search-pred.hpp"
#include <array>
#include <cr/Array.h>

namespace floormat::Search { struct cache; }
namespace floormat::Grid { class PoolRegistry; }
namespace floormat::Grid::Pass { class Pool; }

namespace floormat {

class world;
struct point;
struct path_search_result;

class astar
{
public:
    struct visited;
    struct frontier;
    struct bucket_queue;

    using pred = Search::pred;

    astar();
    ~astar() noexcept;
    void reserve(size_t capacity);

    struct Search::cache* cache();
    Grid::PoolRegistry& pool_registry();

    // todo add simple bresenham short-circuit
    template<int Debug = 0, bool FindClosestOnFailure = true>
    path_search_result Dijkstra(world& w, point from, point to,
                                uint32_t max_dist, Vector2ui own_size,
                                const pred& p);

    template<int Debug = 0, bool FindClosestOnFailure = true>
    path_search_result Dijkstra(world& w, Grid::Pass::Pool& pool, point from, point to,
                                uint32_t max_dist, Vector2ui own_size,
                                const pred& p);

    struct string_pull_result
    {
        // Corners appended, including ones a turn later removed.
        uint32_t corners_added = 0, los_checks = 0;
        bool smoothed = false;
        explicit operator bool() const { return smoothed; }
    };

    // Han, Uras, Koenig, "Toward a String-Pulling Approach to Path Smoothing on Grid Graphs",
    // SoCS 2020, Algorithm 1, over the steps of the raw path, one cell of the pool for own_size
    // each. Not smoothed when it gives up, with smoothings() left empty.
    string_pull_result string_pull(world& w, path_search_result& result, Vector2ui own_size, const pred& p);
    // `pool` is the one the search ran on: steps of another cell size split its segments wrong.
    string_pull_result string_pull(world& w, Grid::Pass::Pool& pool, path_search_result& result,
                                   Vector2ui own_size, const pred& p);

private:
    static constexpr uint32_t initial_capacity = 1u << 17;

    safe_ptr<struct Search::cache> _cache;
    safe_ptr<Grid::PoolRegistry> _pool_registry;
    Array<visited> nodes;
    Array<Array<frontier>> buckets;
    Array<uint32_t> bucket_sizes;
    Array<point> temp_nodes;
    Array<const uint8_t*> chunk_bits;
    Array<uint32_t> insertion_state;
    Array<Range2D> insertion_rects;
};

} // namespace floormat
