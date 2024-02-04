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
#include <bitset>
#include <Corrade/Containers/Array.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/DimensionTraits.h>

namespace floormat {

class world;
struct object;
class chunk;

// todo add pathfinding sub-namespace

namespace detail_astar {

struct chunk_cache;
static constexpr int div_factor = 4;
static constexpr auto div_size = iTILE_SIZE2 / div_factor;
static constexpr auto min_size = Vector2ui(div_size * 2);

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

    std::array<world::neighbor_pair, 8> get_neighbors(world& w, chunk_coords_ ch0);
};
} // namespace detail_astar
struct path_search_result;
enum class path_search_continue : bool { pass = false, blocked = true };

class path_search final
{
    friend struct path_search_result;

public:
    template<typename T>
    requires std::is_arithmetic_v<T>
    struct bbox
    {
        VectorTypeFor<2, T> min, max;

        template<typename U>
        requires std::is_arithmetic_v<U>
        explicit constexpr operator bbox<U>() const {
            using Vec = VectorTypeFor<2, U>;
            return bbox<U>{ Vec(min), Vec(max) };
        }

        constexpr bool operator==(const bbox<T>&) const = default;
    };

    using pred = fu2::function_view<path_search_continue(collision_data) const>;

    static const pred& never_continue() noexcept;
    static const pred& always_continue() noexcept;

    static bool is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable_(chunk* c0, const std::array<world::neighbor_pair, 8>& neighbors,
                             Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, global_coords coord, Vector2b offset, Vector2ui size, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, struct detail_astar::cache& cache, global_coords coord, Vector2b offset, Vector2ui size, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, chunk_coords_ ch0, const bbox<float>& bb, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, struct detail_astar::cache& cache, chunk_coords_ ch0, const bbox<float>& bb, object_id own_id, const pred& p = never_continue());
};

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
