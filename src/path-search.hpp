#pragma once
#include "tile-defs.hpp"
#include "global-coords.hpp"
#include "object-id.hpp"
#include "rotation.hpp"
#include "world.hpp"
#include "compat/function2.fwd.hpp"
#include <array>
#include <bitset>
#include <memory>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/StridedDimensions.h>
#include <Magnum/Math/Vector2.h>

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

struct path_search_result final
{
    friend class path_search;
    path_search_result();

    size_t size() const;
    const global_coords& operator[](size_t index) const;
    explicit operator ArrayView<global_coords>() const;

    const global_coords* begin() const;
    const global_coords* cbegin() const;
    const global_coords* end() const;
    const global_coords* cend() const;
    const global_coords* data() const;

private:
    mutable path_search_result* _next = nullptr;
    std::unique_ptr<global_coords[]> _path;
    size_t _size = 0;
};

enum class path_search_continue : bool { pass = false, blocked = true };

class path_search final
{
    struct neighbors final
    {
        auto begin() const { return neighbors.data(); }
        auto end() const { return neighbors.data() + size; }

        std::array<global_coords, 4> neighbors;
        uint8_t size = 0;
    };

    struct chunk_tiles_cache
    {
        std::bitset<TILE_COUNT> can_go_north{true}, can_go_west{true};
    };

    struct chunk_cache
    {
        Array<chunk_tiles_cache> array;
        Vector2i start, size; // in chunks
    };

    struct obj_position { Vector2 center, size; };

    // todo bucketize by array length
    path_search_result* pool = nullptr;

public:
    chunk_cache cache;
    Array<global_coords> output;

    struct bbox { Vector2 min, max; };

    using pred = fu2::function_view<path_search_continue(collision_data) const>;
    static const pred& never_continue() noexcept;

    void ensure_allocated(chunk_coords a, chunk_coords b);
    void fill_cache(world& w, Vector2i cmin, Vector2i cmax, int8_t z, Vector2ub own_size, object_id own_id, const pred& p = never_continue());
    void fill_cache_(world& w, chunk_coords_ coord, Vector2ub own_size, object_id own_id, const pred& p = never_continue());

    // todo remember to check from.z() == to.z()
    // todo add simple bresenham short-circuit
    Optional<path_search_result> dijkstra(world& w, Vector2ub own_size, object_id own_id, global_coords from, Vector2b from_offset, global_coords to, Vector2b to_offset, const pred& p = never_continue());
    Optional<path_search_result> dijkstra(world& w, const object& obj, global_coords to, Vector2b to_offset, const pred& p = never_continue());

    static bool is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable_(chunk* c0, const std::array<world::neighbor_pair, 8>& neighbors,
                             Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, chunk_coords_ ch0, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, global_coords coord, Vector2b offset, Vector2ub size, object_id own_id, const pred& p = never_continue());

    static bbox make_neighbor_tile_bbox(Vector2i coord, Vector2ub own_size, rotation r);
    static bbox bbox_union(bbox bb, Vector2i coord, Vector2b offset, Vector2ub size);
    static neighbors get_walkable_neighbor_tiles(world& w, global_coords coord, Vector2ub size, object_id own_id, const pred& p = never_continue());
};

} // namespace floormat
