#pragma once
#include "tile-defs.hpp"
#include "global-coords.hpp"
#include "object-id.hpp"
#include "rotation.hpp"
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

struct search_result;
class search;

struct search_result final
{
    friend class search;
    ~search_result();

    size_t size() const;
    const global_coords& operator[](size_t index) const;
    explicit operator ArrayView<global_coords>() const;

    const global_coords* begin() const;
    const global_coords* cbegin() const;
    const global_coords* end() const;
    const global_coords* cend() const;
    const global_coords* data() const;

private:
    mutable search_result* _next;
    std::unique_ptr<global_coords[]> _path;
    size_t _size;
};

class search final
{
    struct neighbors
    {
        auto begin() const { return neighbors.data(); }
        auto end() const { return neighbors.data() + size; }

        std::array<global_coords, 4> neighbors;
        uint8_t size;
    };

    struct chunk_tiles_cache
    {
        std::bitset<TILE_COUNT> is_passable, can_go_north, can_go_west;
    };

    struct chunk_cache
    {
        Array<chunk_tiles_cache> array;
        Vector2i start, size; // in chunks
    };

    struct obj_position { Vector2 center, size; };

    chunk_cache cache;
    Array<global_coords> output;

    // todo bucketize by array length
    search_result* pool = nullptr;

    void ensure_allocated(chunk_coords a, chunk_coords b);

public:
    struct bbox { Vector2 min, max; };

    // todo remember to check from.z() == to.z()
    // todo add simple bresenham short-circuit
    Optional<search_result> operator()(world& w, object_id own_id, global_coords from, Vector2b from_offset, Vector2ub size, global_coords to, Vector2b to_offset);
    Optional<search_result> operator()(world& w, const object& obj, global_coords to, Vector2b to_offset);

    static bool sample_rtree_1(chunk& c, Vector2 min, Vector2 max, object_id own_id);
    static bool sample_rtree(world& w, chunk_coords_ ch0, Vector2 center, Vector2 size, object_id own_id);
    static bool sample_rtree(world& w, global_coords coord, Vector2b offset, Vector2ub size, object_id own_id);

    static bbox make_neighbor_tile_bbox(Vector2i coord, Vector2ub own_size, rotation r);
    static neighbors get_walkable_neighbor_tiles(world& w, global_coords pos, Vector2 size, object_id own_id);
};

} // namespace floormat