#pragma once
#include "tile.hpp"
#include "tile-iterator.hpp"
#include "scenery.hpp"
#include <concepts>
#include <type_traits>
#include <array>
#include <memory>
#include <Magnum/GL/Mesh.h>
#include "RTree.h"

namespace floormat {

struct anim_atlas;

enum class collision : std::uint8_t {
    view, shoot, move,
};

enum class collision_type : std::uint8_t {
    none, entity, scenery, geometry,
};

struct collision_data final {
    std::uint64_t tag       : 2;
    std::uint64_t pass      : 2;
    std::uint64_t data      : 60;
};

struct chunk final
{
    friend struct tile_ref;

    tile_ref operator[](std::size_t idx) noexcept;
    tile_proto operator[](std::size_t idx) const noexcept;
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

    chunk() noexcept;
    ~chunk() noexcept;
    chunk(const chunk&) = delete;
    chunk& operator=(const chunk&) = delete;
    chunk(chunk&&) noexcept;
    chunk& operator=(chunk&&) noexcept;

    void mark_ground_modified() noexcept;
    void mark_walls_modified() noexcept;
    void mark_scenery_modified() noexcept;
    bool is_passability_modified() const noexcept;
    void mark_modified() noexcept;

    struct ground_mesh_tuple final {
        GL::Mesh& mesh;
        const ArrayView<const std::uint8_t> ids;
        const std::size_t size;
    };
    struct wall_mesh_tuple final {
        GL::Mesh& mesh;
        const ArrayView<const std::uint16_t> ids;
        const std::size_t size;
    };

    struct scenery_mesh_tuple final {
        GL::Mesh& mesh;
        const ArrayView<const std::uint8_t> ids;
        const std::size_t size;
    };

    ground_mesh_tuple ensure_ground_mesh() noexcept;
    tile_atlas* ground_atlas_at(std::size_t i) const noexcept;

    wall_mesh_tuple ensure_wall_mesh() noexcept;
    tile_atlas* wall_atlas_at(std::size_t i) const noexcept;

    scenery_mesh_tuple ensure_scenery_mesh() noexcept;
    std::shared_ptr<anim_atlas>& scenery_atlas_at(std::size_t i) noexcept;
    scenery& scenery_at(std::size_t i) noexcept;

    void ensure_passability() noexcept;

    using RTree = ::RTree<std::uint64_t, float, 2, float>;

    const RTree* rtree() const noexcept;
    RTree* rtree() noexcept;

    template<std::invocable<tile_ref&> F> void with_scenery_bbox_update(tile_ref t, F&& fun);
    template<std::invocable<> F> void with_scenery_bbox_update(std::size_t i, F&& fun);

private:
    std::array<std::shared_ptr<tile_atlas>, TILE_COUNT> _ground_atlases;
    std::array<std::uint8_t, TILE_COUNT> ground_indexes = {};
    std::array<variant_t, TILE_COUNT> _ground_variants = {};
    std::array<std::shared_ptr<tile_atlas>, TILE_COUNT*2> _wall_atlases;
    std::array<std::uint16_t, TILE_COUNT*2> wall_indexes = {};
    std::array<variant_t, TILE_COUNT*2> _wall_variants = {};
    std::array<std::shared_ptr<anim_atlas>, TILE_COUNT> _scenery_atlases;
    std::array<std::uint8_t, TILE_COUNT> scenery_indexes = {};
    std::array<scenery, TILE_COUNT> _scenery_variants = {};

    GL::Mesh ground_mesh{NoCreate}, wall_mesh{NoCreate}, scenery_mesh{NoCreate};

    RTree _rtree;

    mutable bool _maybe_empty      : 1 = true,
                 _ground_modified  : 1 = true,
                 _walls_modified   : 1 = true,
                 _scenery_modified : 1 = true,
                 _pass_modified    : 1 = true;

    struct bbox final // NOLINT(cppcoreguidelines-pro-type-member-init)
    {
        std::uint64_t id;
        Vector2i start, end;

        bool operator==(const bbox& other) const noexcept;
    };
    bool _bbox_for_scenery(std::size_t i, bbox& value) noexcept;
    void _remove_bbox(const bbox& x);
    void _add_bbox(const bbox& x);
    void _replace_bbox(const bbox& x0, const bbox& x, bool b0, bool b);
};

template<std::invocable<tile_ref&> F>
void chunk::with_scenery_bbox_update(tile_ref t, F&& fun)
{
    if (is_passability_modified())
        return fun(t);
    else
    {
        bbox x0, x;
        std::size_t i = t.index();
        bool b0 = _bbox_for_scenery(i, x0);
        fun(t);
        _replace_bbox(x0, x, b0, _bbox_for_scenery(i, x));
    }
}

template<std::invocable<> F>
void chunk::with_scenery_bbox_update(std::size_t i, F&& fun)
{
    if (is_passability_modified())
        return fun();
    else
    {
        bbox x0, x;
        bool b0 = _bbox_for_scenery(i, x0);
        fun();
        _replace_bbox(x0, x, b0, _bbox_for_scenery(i, x));
    }
}

} // namespace floormat
