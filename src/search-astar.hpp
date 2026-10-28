#pragma once
#include "compat/safe-ptr.hpp"
#include "search-pred.hpp"
#include "tile-defs.hpp"
#include <cr/Array.h>
#include <mg/Vector2.h>

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
    struct goal_seed;
    struct endpoint;
    struct chunk_bitmaps;

    using pred = Search::pred;

    // cell size of the pools searched by the overloads that take no pool
    static constexpr uint32_t default_div_size = (uint32_t)tile_size_xy/4;

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
    struct seed_search;
    struct closest;

    static constexpr uint32_t initial_capacity = 1u << 17;

    bucket_queue make_queue(uint32_t bucket_count);
    chunk_bitmaps make_bitmaps(world& w, Grid::Pass::Pool& pool, const pred& p);
    bool seed_ring(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& start, point to, uint32_t max_dist);
    visited run_seed_search(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& start, point to, uint32_t max_dist);
    visited run_goal_seed_search(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& start, const endpoint& goal);
    template<int Debug, bool FindClosestOnFailure>
    closest expand_nodes(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& goal, uint32_t max_dist, closest best);
    template<int Debug>
    path_search_result make_result(const closest& best, point from, point to, uint32_t max_dist, float time);

    safe_ptr<struct Search::cache> _cache;
    safe_ptr<Grid::PoolRegistry> _pool_registry;
    Array<visited> nodes;
    uint32_t node_count = 0;
    Array<Array<frontier>> buckets;
    Array<uint32_t> bucket_sizes;
    Array<point> temp_nodes;
    Array<const uint8_t*> chunk_bits;
    Array<uint32_t> seed_search_state;
    Array<Range2D> seed_rects;
    Array<goal_seed> goal_seeds;
    Array<uint32_t> goal_seed_index;
    Vector2i goal_seed_off;
    uint32_t goal_seed_n = 0;
};

} // namespace floormat
