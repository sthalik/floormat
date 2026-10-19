#pragma once
#include "compat/safe-ptr.hpp"
#include "search-constants.hpp"
#include "search-pred.hpp"
#include "object-id.hpp"
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
    // SoCS 2020, Algorithm 1, over the Search::div_size steps of the raw path. Not smoothed when it
    // gives up, with smoothings() left empty.
    string_pull_result string_pull(world& w, path_search_result& result, Vector2ui own_size, const pred& p);

private:
    static constexpr auto initial_capacity = TILE_COUNT * 32 * Search::div_factor*Search::div_factor;
    // The seeds' f can spread by about 134, more than a step adds.
    static constexpr uint32_t bucket_count = 256;

    safe_ptr<struct Search::cache> _cache;
    safe_ptr<Grid::PoolRegistry> _pool_registry;
    Array<visited> nodes;
    std::array<Array<frontier>, bucket_count> buckets;
    Array<point> temp_nodes;
    Array<const uint8_t*> chunk_bits;
};

} // namespace floormat
