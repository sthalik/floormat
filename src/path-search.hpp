#pragma once
#include "global-coords.hpp"
#include "object-id.hpp"
#include "rotation.hpp"
#include "world.hpp"
#include "compat/function2.fwd.hpp"
#include "path-search-result.hpp"
#include "compat/int-hash.hpp"
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
struct path_search_result;

enum class path_search_continue : bool { pass = false, blocked = true };

struct astar
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(astar);

    struct visited
    {
        uint32_t dist = (uint32_t)-1;
        uint32_t prev = (uint32_t)-1;
        global_coords coord;
        Vector2b offset;
        bool expanded = false;

    };
    struct edge
    {
        global_coords from, to;
        Vector2b from_offset, to_offset;

        bool operator==(const edge& other) const;
    };

    struct point_hash { size_t operator()(point pt) const; };
    struct edge_hash { size_t operator()(const edge& e) const; };

    void reserve(size_t capacity)
    {
        nodes.reserve(capacity);
        indexes.reserve(capacity);
        Q.reserve(capacity);
    }
    astar()
    {
        constexpr auto capacity = TILE_COUNT * 16;
        indexes.max_load_factor(.4f);
        reserve(capacity);
    }
    void clear()
    {
        nodes.clear();
        indexes.clear();
        Q.clear();
    }

    std::vector<visited> nodes;
    tsl::robin_set<edge, edge_hash> edges;
    tsl::robin_map<point, uint32_t, point_hash> indexes;
    std::vector<uint32_t> Q;
};

class path_search final
{
    friend struct path_search_result;

public:
    static constexpr int subdivide_factor = 4;
    static constexpr auto div_size = iTILE_SIZE2 / subdivide_factor;
    static constexpr auto min_size = div_size / 2;

    struct neighbors final
    {
        auto begin() const { return data.data(); }
        auto end() const { return data.data() + size; }

        std::array<global_coords, 5> data;
        uint8_t size = 0;

        operator ArrayView<const global_coords>() const;
    };

    template<typename T> struct bbox { VectorTypeFor<2, T> min, max; };

    using pred = fu2::function_view<path_search_continue(collision_data) const>;

    struct astar astar;

    static const pred& never_continue() noexcept;
    static const pred& always_continue() noexcept;

    // todo add simple bresenham short-circuit
    path_search_result Dijkstra(world& w, Vector2ub own_size, object_id own_id, point from, point to, const pred& p = never_continue());

    static bool is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable_(chunk* c0, const std::array<world::neighbor_pair, 8>& neighbors,
                             Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, chunk_coords_ ch0, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, global_coords coord, Vector2b offset, Vector2ub size, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, chunk_coords_ ch0, const bbox<float>& bb, object_id own_id, const pred& p = never_continue());

    // todo move to test/path-search.cpp
    static bbox<float> neighbor_tile_bbox(Vector2i coord, Vector2ub own_size, Vector2ub div, rotation r);
    static neighbors neighbor_tiles(world& w, global_coords coord, Vector2ub size, object_id own_id, const pred& p = never_continue());
};

} // namespace floormat
