#pragma once
#include "compat/safe-ptr.hpp"
#include "search-pred.hpp"
#include "tile-defs.hpp"
#include <array>
#include <cr/Array.h>
#include <mg/Vector2.h>

namespace floormat::Search { struct cache; }
namespace floormat::Grid { class GridRegistry; }
namespace floormat::Grid::Pass { class Pool; }
namespace floormat::Grid::Object { class Pool; }

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
    static constexpr u32 default_div_size = (u32)tile_size_xy/4;

    astar();
    ~astar() noexcept;

    struct Search::cache* cache();
    Grid::GridRegistry& pool_registry();
    u32 last_node_count() const { return node_count; }

    // To route around critters and doors, p must block them, as Search::blocks_all_but(self) does.
    // todo add simple bresenham short-circuit
    template<int Debug = 0, bool FindClosestOnFailure = true>
    path_search_result Dijkstra(world& w, point from, point to,
                                u32 max_dist, Vector2ui own_size,
                                const pred& p);

    // Only pool's bits and p decide what blocks.
    template<int Debug = 0, bool FindClosestOnFailure = true>
    path_search_result Dijkstra(world& w, Grid::Pass::Pool& pool, point from, point to,
                                u32 max_dist, Vector2ui own_size, const pred& p);
    template<int Debug = 0, bool FindClosestOnFailure = true>
    path_search_result Dijkstra(world& w, Grid::Object::Pool& objects, point from, point to,
                                u32 max_dist, Vector2ui own_size, const pred& p);

    struct string_pull_result
    {
        // corners_added counts corners appended, including ones removed later.
        u32 corners_added = 0, los_checks = 0;
        bool smoothed = false;
        explicit operator bool() const { return smoothed; }
    };

    // Returns whether it smoothed result's path by Algorithm 1 of Han, Uras and Koenig (SoCS 2020), run on
    // raw_path() split into moves one cell long, with p as for Dijkstra(). On failure, smoothings() is left empty.
    string_pull_result string_pull(world& w, path_search_result& result, Vector2ui own_size, const pred& p);
    // `pool` must be the one the search ran on. Another cell size fails the assert in string_pull().
    string_pull_result string_pull(world& w, Grid::Pass::Pool& pool, path_search_result& result,
                                   Vector2ui own_size, const pred& p);
    string_pull_result string_pull(world& w, Grid::Object::Pool& objects, path_search_result& result,
                                   Vector2ui own_size, const pred& p);

private:
    struct seed_search;
    struct closest;

    static constexpr u32 initial_capacity = 1u << 17;

    template<int Debug, bool FindClosestOnFailure>
    path_search_result Dijkstra(world& w, Grid::Pass::Pool& pool, Grid::Object::Pool* objects, point from, point to,
                                u32 max_dist, Vector2ui own_size, const pred& p);
    string_pull_result string_pull(world& w, Grid::Pass::Pool& pool, Grid::Object::Pool* objects,
                                   path_search_result& result, Vector2ui own_size, const pred& p);

    bucket_queue make_queue(u32 bucket_count);
    chunk_bitmaps make_bitmaps(world& w, Grid::Pass::Pool& pool, const pred& p, Grid::Object::Pool* objects);
    bool seed_ring(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& start, point to, u32 max_dist);
    closest run_seed_search(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& start, point to, u32 max_dist,
                            closest best);
    closest run_goal_seed_search(chunk_bitmaps& bm, const endpoint& start, const endpoint& goal, closest best);
    u32 add_path_to_goal(point goal, point pt, u32 prev);
    template<int Debug, bool FindClosestOnFailure>
    closest expand_nodes(chunk_bitmaps& bm, bucket_queue& Q, const endpoint& goal, u32 max_dist, closest best, u32 max_pops);
    template<int Debug>
    path_search_result make_result(const closest& best, point from, point to, u32 max_dist, float time);

    safe_ptr<struct Search::cache> _cache;
    safe_ptr<Grid::GridRegistry> _pool_registry;
    // In _cache's memory, valid from its allocate() to the next.
    ArrayView<visited> nodes;
    u32 node_count = 0;
    Array<Array<frontier>> buckets;
    Array<u32> bucket_sizes;
    Array<point> temp_nodes;
    Array<const usize*> chunk_bits;
    Array<usize> masked_bits;
    std::array<Array<bool>, 4> entry_blocked;
    Array<u32> seed_search_state, goal_seed_search_state;
    // Per position of the start's seed search window, the node add_path() put there, or -1.
    Array<u32> seed_node_index;
    Array<Array<frontier>> seed_buckets;
    Array<u32> seed_bucket_sizes;
    Array<Range2D> seed_rects;
    Array<goal_seed> goal_seeds;
    Array<u32> goal_seed_index;
    Vector2i goal_seed_off;
    u32 goal_seed_n = 0;
};

} // namespace floormat
