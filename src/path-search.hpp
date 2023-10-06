#pragma once
#include "tile-defs.hpp"
#include "global-coords.hpp"
#include "object-id.hpp"
#include "rotation.hpp"
#include "world.hpp"
#include "compat/function2.fwd.hpp"
#include "path-search-result.hpp"
#include <memory>
#include <array>
#include <vector>
#include <bitset>
#include <Corrade/Containers/Array.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/DimensionTraits.h>

namespace Corrade::Containers {
template<typename T> class Optional;
template<typename T, typename U> class Pair;
} // namespace Corrade::Containers

namespace floormat {

struct world;
struct object;
struct chunk;

struct path_search_result;
class path_search;

struct search_astar final
{
    struct vertex_tuple
    {
        static constexpr auto INF = (uint32_t)-1;

        const chunk *c1 = nullptr, *c2 = nullptr;
        uint32_t dist = 0;
    };

    [[nodiscard]] bool reserve(chunk_coords_ c1, chunk_coords_ c2);
    void insert(const chunk& c1, const chunk& c2);
    vertex_tuple delete_min();
    void add_edges();
    bool find(chunk_coords_);

    std::vector<vertex_tuple> Q;
    std::vector<global_coords> output;
};

enum class path_search_continue : bool { pass = false, blocked = true };

class path_search final
{
    friend struct path_search_result;

    // todo bucketize by array length
    path_search_result* pool = nullptr;

public:
    static constexpr int subdivide_factor = 4;
    static constexpr auto min_size = iTILE_SIZE2 / subdivide_factor / 2;
    static constexpr size_t tile_count = Vector2i(subdivide_factor * TILE_MAX_DIM).product();

    struct neighbors final
    {
        auto begin() const { return neighbors.data(); }
        auto end() const { return neighbors.data() + size; }

        std::array<global_coords, 4> neighbors;
        uint8_t size = 0;
    };

    struct chunk_tiles_cache
    {
        std::bitset<tile_count> can_go_north{true}, can_go_west{true};
    };

    struct chunk_cache
    {
        Array<chunk_tiles_cache> array;
        Vector2i start, size; // in chunks
    };

    struct obj_position { Vector2 center, size; };

    template<typename T> struct bbox { VectorTypeFor<2, T> min, max; };

    chunk_cache cache;

    using pred = fu2::function_view<path_search_continue(collision_data) const>;
    static const pred& never_continue() noexcept;
    static const pred& always_continue() noexcept;

    void ensure_allocated(chunk_coords a, chunk_coords b);
    void fill_cache(world& w, Vector2i cmin, Vector2i cmax, int8_t z, Vector2ub own_size, object_id own_id, const pred& p = never_continue());
    void fill_cache_(world& w, chunk_coords_ coord, Vector2ub own_size, object_id own_id, const pred& p = never_continue());

    // todo remember to check from.z() == to.z()
    // todo add simple bresenham short-circuit
    Optional<path_search_result> Dijkstra(world& w, Vector2ub own_size, object_id own_id, global_coords from, Vector2b from_offset, global_coords to, Vector2b to_offset, const pred& p = never_continue());
    Optional<path_search_result> Dijkstra(world& w, const object& obj, global_coords to, Vector2b to_offset, const pred& p = never_continue());

    static bool is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable_(chunk* c0, const std::array<world::neighbor_pair, 8>& neighbors,
                             Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, chunk_coords_ ch0, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, global_coords coord, Vector2b offset, Vector2ub size, object_id own_id, const pred& p = never_continue());

    static bbox<float> neighbor_tile_bbox(Vector2i coord, Vector2ub own_size, Vector2ub div, rotation r);
    static bbox<float> bbox_union(bbox<float> bb, Vector2i coord, Vector2b offset, Vector2ub size);
    static neighbors neighbor_tiles(world& w, global_coords coord, Vector2ub size, object_id own_id, const pred& p = never_continue());
};

} // namespace floormat
