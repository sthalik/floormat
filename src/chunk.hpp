#pragma once
#include "object-id.hpp"
#include "tile.hpp"
#include "local-coords.hpp"
#include "src/RTree.h"
#include "global-coords.hpp"
#include "wall-defs.hpp"
#include <type_traits>
#include <array>
#include <Corrade/Containers/Pointer.h>
#include <Magnum/GL/Mesh.h>

namespace floormat {

class anim_atlas;
class wall_atlas;
struct object;
struct object_proto;
class tile_iterator;
class tile_const_iterator;

enum class collision : unsigned char {
    view, shoot, move,
};

enum class collision_type : unsigned char {
    none, object, scenery, geometry,
};

struct collision_data final {
    uint64_t tag       : 2;
    uint64_t pass      : 2;
    uint64_t data      : 60;
};

struct chunk final
{
    friend struct tile_ref;
    friend struct object;
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

    explicit chunk(struct world& w, chunk_coords_ ch) noexcept;
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
        const ArrayView<const uint_fast16_t> indexes;
        const size_t size;
    };
    struct topo_sort_data;
    struct object_draw_order;
    struct scenery_mesh_tuple;
    struct scenery_scratch_buffers;

    struct vertex {
        Vector3 position;
        Vector2 texcoords;
        float depth = -1;
    };

    using RTree = ::RTree<object_id, float, 2, float>;

    void ensure_alloc_ground();
    void ensure_alloc_walls();
    ground_mesh_tuple ensure_ground_mesh() noexcept;
    tile_atlas* ground_atlas_at(size_t i) const noexcept;
    wall_atlas* wall_atlas_at(size_t i) const noexcept;
    wall_mesh_tuple ensure_wall_mesh() noexcept;

    scenery_mesh_tuple ensure_scenery_mesh(scenery_scratch_buffers buffers) noexcept;
    scenery_mesh_tuple ensure_scenery_mesh() noexcept;

    void ensure_passability() noexcept;
    RTree* rtree() noexcept;
    struct world& world() noexcept { return *_world; }

    [[nodiscard]] bool can_place_object(const object_proto& proto, local_coords pos);

    void add_object(const std::shared_ptr<object>& e);
    void add_object_unsorted(const std::shared_ptr<object>& e);
    void sort_objects();
    void remove_object(size_t i);
    const std::vector<std::shared_ptr<object>>& objects() const;

    // for drawing only
    static constexpr size_t max_wall_quad_count =
        TILE_COUNT*Wall::Direction_COUNT*Wall::Group_COUNT;

private:
    struct ground_stuff
    {
        // todo remove "_ground" prefix
        std::array<std::shared_ptr<tile_atlas>, TILE_COUNT> _ground_atlases;
        std::array<uint8_t, TILE_COUNT> ground_indexes = {};
        std::array<variant_t, TILE_COUNT> _ground_variants = {};
    };

    struct wall_stuff
    {
        std::array<std::shared_ptr<wall_atlas>, 2*TILE_COUNT> atlases;
        std::array<variant_t, 2*TILE_COUNT> variants;
        std::array<uint_fast16_t, max_wall_quad_count> mesh_indexes;
    };

    Pointer<ground_stuff> _ground;
    Pointer<wall_stuff> _walls;
    std::vector<std::shared_ptr<object>> _objects;
    struct world* _world;
    GL::Mesh ground_mesh{NoCreate}, wall_mesh{NoCreate}, scenery_mesh{NoCreate};
    RTree _rtree;
    chunk_coords_ _coord;

    mutable bool _maybe_empty            : 1 = true,
                 _ground_modified        : 1 = true,
                 _walls_modified         : 1 = true,
                 _scenery_modified       : 1 = true,
                 _pass_modified          : 1 = true,
                 _teardown               : 1 = false,
                 _objects_sorted         : 1 = true;

    void ensure_scenery_buffers(scenery_scratch_buffers bufs);
    static topo_sort_data make_topo_sort_data(object& e, uint32_t mesh_idx);

    struct bbox final // NOLINT(cppcoreguidelines-pro-type-member-init)
    {
        object_id id;
        Vector2i start, end;

        bool operator==(const bbox& other) const noexcept;
    };

    static bool _bbox_for_scenery(const object& s, bbox& value) noexcept;
    static bool _bbox_for_scenery(const object& s, local_coords local, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, bbox& value) noexcept;
    void _remove_bbox(const bbox& x);
    void _add_bbox(const bbox& x);
    void _replace_bbox(const bbox& x0, const bbox& x, bool b0, bool b);
    GL::Mesh make_wall_mesh();

    template<size_t N> static std::array<std::array<UnsignedShort, 6>, N*TILE_COUNT> make_index_array(size_t max);
};

} // namespace floormat
