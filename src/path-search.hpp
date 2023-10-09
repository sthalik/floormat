#pragma once
#include "global-coords.hpp"
#include "object-id.hpp"
#include "rotation.hpp"
#include "world.hpp"
#include "compat/function2.fwd.hpp"
#include "path-search-result.hpp"
#include "compat/int-hash.hpp"
#include "point.hpp"
#include <bit>
#include <array>
#include <Magnum/Math/Vector2.h>
#include <Magnum/DimensionTraits.h>
#include <tsl/robin_map.h>
#include <tsl/robin_set.h>

namespace Corrade::Containers {
//template<typename T> class Optional;
//template<typename T, typename U> class Pair;
template<typename T> class ArrayView;
} // namespace Corrade::Containers

namespace floormat {

struct world;
struct object;
struct chunk;

// todo add pathfinding sub-namespace

struct path_search_result;
enum class path_search_continue : bool { pass = false, blocked = true };

class path_search final
{
    friend struct path_search_result;

public:
    static constexpr int div_factor = 4;
    static constexpr auto div_size = iTILE_SIZE2 / div_factor;
    static constexpr auto min_size = Vector2ui(div_size * 2);

    template<typename T> struct bbox { VectorTypeFor<2, T> min, max; };

    using pred = fu2::function_view<path_search_continue(collision_data) const>;

    static const pred& never_continue() noexcept;
    static const pred& always_continue() noexcept;

    static bool is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable_(chunk* c0, const std::array<world::neighbor_pair, 8>& neighbors,
                             Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, chunk_coords_ ch0, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, global_coords coord, Vector2b offset, Vector2ub size, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, global_coords coord, Vector2b offset, Vector2ui size, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, chunk_coords_ ch0, const bbox<float>& bb, object_id own_id, const pred& p = never_continue());
};

struct astar
{
    struct visited
    {
        uint32_t dist = (uint32_t)-1;
        uint32_t prev = (uint32_t)-1;
        global_coords coord;
        Vector2b offset;
    };

    using pred = path_search::pred;
    template<typename T> using bbox = path_search::bbox<T>;
    struct point_hash { size_t operator()(point pt) const; };

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(astar);

    astar();
    void reserve(size_t capacity);
    void clear();

    // todo add simple bresenham short-circuit
    path_search_result Dijkstra(world& w, point from, point to,
                                object_id own_id, uint32_t max_dist, Vector2ub own_size,
                                int debug = 0, const pred& p = path_search::never_continue());

//#define FM_ASTAR_NO_EDGE_CACHE

private:
    static constexpr auto div_factor = (int8_t)path_search::div_factor;
    static constexpr auto initial_capacity = TILE_COUNT * 16 * div_factor*div_factor;

    void add_to_heap(uint32_t id);
    uint32_t pop_from_heap();

#ifndef FM_ASTAR_NO_EDGE_CACHE
    struct edge
    {
        global_coords from, to;
        Vector2b from_offset, to_offset;

        bool operator==(const edge& other) const;
    };
    enum class edge_status : uint8_t { unknown, good, bad, };
    struct edge_hash { size_t operator()(const edge& e) const; };
    static edge make_edge(const point& a, const point& b);

    tsl::robin_map<edge, edge_status, edge_hash> edges;
#endif

    std::vector<visited> nodes;
    tsl::robin_map<point, uint32_t, point_hash> indexes;
    std::vector<uint32_t> Q;
};

} // namespace floormat
