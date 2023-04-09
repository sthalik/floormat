#pragma once
#include "object-id.hpp"
#include "tile.hpp"
#include "local-coords.hpp"
#include "src/RTree.h"
#include <type_traits>
#include <array>
#include <memory>
#include <Magnum/GL/Mesh.h>

namespace Corrade::Containers { template<typename T, typename D> class Array; }

namespace floormat {

struct anim_atlas;
struct entity;
struct entity_proto;
class tile_iterator;
class tile_const_iterator;

enum class collision : unsigned char {
    view, shoot, move,
};

enum class collision_type : unsigned char {
    none, entity, scenery, geometry,
};

struct collision_data final {
    uint64_t tag       : 2;
    uint64_t pass      : 2;
    uint64_t data      : 60;
};

struct chunk final
{
    friend struct tile_ref;
    friend struct entity;
    friend struct world;

    tile_ref operator[](size_t idx) noexcept;
    tile_proto operator[](size_t idx) const noexcept;
    tile_ref operator[](local_coords xy) noexcept;
    tile_proto operator[](local_coords xy) const noexcept;

    using iterator = tile_iterator;
    using const_iterator = tile_const_iterator;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    bool empty(bool force = false) const noexcept;

    explicit chunk(struct world& w) noexcept;
    ~chunk() noexcept;
    chunk(const chunk&) = delete;
    chunk& operator=(const chunk&) = delete;
    chunk(chunk&&) noexcept;
    chunk& operator=(chunk&&) noexcept;

    void mark_ground_modified() noexcept;
    void mark_walls_modified() noexcept;
    void mark_scenery_modified() noexcept;
    void mark_passability_modified() noexcept;
    void mark_modified() noexcept;

    bool is_passability_modified() const noexcept;
    bool is_scenery_modified() const noexcept;

    struct ground_mesh_tuple final {
        GL::Mesh& mesh;
        const ArrayView<const uint8_t> ids;
        const size_t size;
    };
    struct wall_mesh_tuple final {
        GL::Mesh& mesh;
        const ArrayView<const uint16_t> ids;
        const size_t size;
    };
    struct topo_sort_data;
    struct entity_draw_order;
    struct scenery_mesh_tuple;

    struct vertex {
        Vector3 position;
        Vector2 texcoords;
        float depth = -1;
    };

    using RTree = ::RTree<object_id, float, 2, float>;

    ground_mesh_tuple ensure_ground_mesh() noexcept;
    tile_atlas* ground_atlas_at(size_t i) const noexcept;
    wall_mesh_tuple ensure_wall_mesh() noexcept;
    tile_atlas* wall_atlas_at(size_t i) const noexcept;
    scenery_mesh_tuple ensure_scenery_mesh(Array<entity_draw_order>&& array) noexcept;
    scenery_mesh_tuple ensure_scenery_mesh(Array<entity_draw_order>& array) noexcept;

    void ensure_passability() noexcept;
    RTree* rtree() noexcept;
    struct world& world() noexcept { return *_world; }

    [[nodiscard]] bool can_place_entity(const entity_proto& proto, local_coords pos);

    void add_entity(const std::shared_ptr<entity>& e);
    void add_entity_unsorted(const std::shared_ptr<entity>& e);
    void sort_entities();
    void remove_entity(size_t i);
    const std::vector<std::shared_ptr<entity>>& entities() const;

private:
    std::array<std::shared_ptr<tile_atlas>, TILE_COUNT> _ground_atlases;
    std::array<uint8_t, TILE_COUNT> ground_indexes = {};
    std::array<variant_t, TILE_COUNT> _ground_variants = {};
    std::array<std::shared_ptr<tile_atlas>, TILE_COUNT*2> _wall_atlases;
    std::array<uint16_t, TILE_COUNT*2> wall_indexes = {};
    std::array<variant_t, TILE_COUNT*2> _wall_variants = {};
    std::vector<std::shared_ptr<entity>> _entities;

    std::vector<std::array<UnsignedShort, 6>> scenery_indexes;
    std::vector<std::array<vertex, 4>> scenery_vertexes;

    struct world* _world;
    GL::Mesh ground_mesh{NoCreate}, wall_mesh{NoCreate};

    RTree _rtree;

    mutable bool _maybe_empty           : 1 = true,
                 _ground_modified       : 1 = true,
                 _walls_modified        : 1 = true,
                 _scenery_modified      : 1 = true,
                 _pass_modified         : 1 = true,
                 _teardown              : 1 = false,
                 _entities_sorted       : 1 = true;

    void ensure_scenery_draw_array(Array<entity_draw_order>& array);
    static topo_sort_data make_topo_sort_data(entity& e);

    struct bbox final // NOLINT(cppcoreguidelines-pro-type-member-init)
    {
        object_id id;
        Vector2i start, end;

        bool operator==(const bbox& other) const noexcept;
    };
    static bool _bbox_for_scenery(const entity& s, bbox& value) noexcept;
    static bool _bbox_for_scenery(const entity& s, local_coords local, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, bbox& value) noexcept;
    void _remove_bbox(const bbox& x);
    void _add_bbox(const bbox& x);
    void _replace_bbox(const bbox& x0, const bbox& x, bool b0, bool b);
    GL::Mesh make_wall_mesh(size_t count);
};

} // namespace floormat
