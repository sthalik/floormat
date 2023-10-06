#pragma once
#include "tile-defs.hpp"
#include "global-coords.hpp"
#include "object-id.hpp"
#include "rotation.hpp"
#include "world.hpp"
#include "compat/function2.fwd.hpp"
#include "path-search-astar.hpp"
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

struct astar_edge;
struct astar_hash;
struct astar;

enum class path_search_continue : bool { pass = false, blocked = true };

class path_search final
{
    friend struct path_search_result;

    // todo bucketize by array length
    struct astar astar;

public:
    static constexpr int subdivide_factor = 4;
    static constexpr auto min_size = iTILE_SIZE2 / subdivide_factor;
    static constexpr size_t tile_count = Vector2i(subdivide_factor * TILE_MAX_DIM).product();

    struct neighbors final
    {
        auto begin() const { return data.data(); }
        auto end() const { return data.data() + size; }

        std::array<global_coords, 5> data;
        uint8_t size = 0;

        operator ArrayView<const global_coords>() const;
    };

#if 0
    struct chunk_tiles_cache
    {
        std::bitset<tile_count> can_go_north{true}, can_go_west{true};
    };

    struct chunk_cache
    {
        Array<chunk_tiles_cache> array;
        Vector2i start, size; // in chunks
    };
#endif

    struct obj_position { Vector2 center, size; };

    template<typename T> struct bbox { VectorTypeFor<2, T> min, max; };

#if 0
    chunk_cache cache;
#endif

    using pred = fu2::function_view<path_search_continue(collision_data) const>;
    static const pred& never_continue() noexcept;
    static const pred& always_continue() noexcept;

#if 0
    size_t cache_chunk_index(chunk_coords coord);
    static size_t cache_tile_index(local_coords tile, Vector2i subdiv);

    void ensure_allocated(chunk_coords a, chunk_coords b);
    void fill_cac`he(world& w, Vector2i cmin, Vector2i cmax, int8_t z, Vector2ub own_size, object_id own_id, const pred& p = never_continue());
    void fill_cache_(world& w, chunk_coords_ coord, Vector2ub own_size, object_id own_id, const pred& p = never_continue());
#endif

    // todo add simple bresenham short-circuit
    path_search_result Dijkstra(world& w, Vector2ub own_size, object_id own_id, global_coords from, Vector2b from_offset, global_coords to, Vector2b to_offset, const pred& p = never_continue());
    path_search_result Dijkstra(world& w, const object& obj, global_coords to, Vector2b to_offset, const pred& p = never_continue());

    static bool is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable_(chunk* c0, const std::array<world::neighbor_pair, 8>& neighbors,
                             Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, chunk_coords_ ch0, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, global_coords coord, Vector2b offset, Vector2ub size, object_id own_id, const pred& p = never_continue());

    static bbox<float> neighbor_tile_bbox(Vector2i coord, Vector2ub own_size, Vector2ub div, rotation r);
    template<typename T> requires std::is_arithmetic_v<T> static bbox<T> bbox_union(bbox<T> bb, Vector2i coord, Vector2b offset, Vector2ub size);
    static bbox<int> bbox_union(bbox<int> bb1, bbox<int> bb2);
    static neighbors neighbor_tiles(world& w, global_coords coord, Vector2ub size, object_id own_id, const pred& p = never_continue());
};

} // namespace floormat
