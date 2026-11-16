#pragma once
#include "object-id.hpp"
#include "tile.hpp"
#include "local-coords.hpp"
#include "RTree-fwd.h"
#include "global-coords.hpp"
#include "search-pred.hpp"
#include "script-enums.hpp"
#include "sprite-list.hpp"
#include "pass-through.hpp"
#include "object-storage.hpp"
#include <array>
#include <cr/Array.h>
#include <cr/Pointer.h>
#include <mg/Range.h>

namespace floormat { class world; }

namespace floormat::Grid::detail {
struct grid_slots;
struct PoolBase;
void mark_grids_stale(world& w, chunk_coords_ coord) noexcept;
} // namespace floormat::Grid::detail

namespace floormat {

class anim_atlas;
class wall_atlas;
struct object;
struct object_proto;
class SpriteBatch;

class chunk final
{
public:
    template<typename Chunk> friend struct tile_ref_;
    friend struct object;
    friend class world;
    friend struct Grid::detail::PoolBase;
    friend void Grid::detail::mark_grids_stale(world& w, chunk_coords_ coord) noexcept;

    tile_ref operator[](usize idx) noexcept;
    const_tile_ref operator[](usize idx) const noexcept;
    tile_ref operator[](local_coords xy) noexcept;
    const_tile_ref operator[](local_coords xy) const noexcept;

    chunk_coords_ coord() const noexcept;
    Optional<tile_ref> at_offset(local_coords pos, Vector2i off);
    Optional<tile_ref> at_offset(tile_ref r, Vector2i off);
    Optional<const_tile_ref> at_offset(local_coords pos, Vector2i off) const;
    Optional<const_tile_ref> at_offset(const_tile_ref r, Vector2i off) const;

    bool empty() const noexcept;
    bool empty(bool force) noexcept;

    explicit chunk(class world& w, chunk_coords_ ch) noexcept;
    ~chunk() noexcept;
    chunk(const chunk&) = delete;
    chunk& operator=(const chunk&) = delete;
    chunk(chunk&&) noexcept = delete;
    chunk& operator=(chunk&&) noexcept = delete;

    void mark_ground_modified() noexcept;
    void mark_walls_modified() noexcept;
    void mark_scenery_modified() noexcept;
    void mark_passability_modified() noexcept;
    /// @brief Call after a critter or a dynamic scenery object, such as a door, is added, removed, moved, turned
    /// or changes pass mode. Static scenery calls mark_passability_modified() instead.
    void mark_objects_modified() noexcept;
    /// Call on each neighbour of a chunk whose hole changed. It marks no grid, so that chunk also needs
    /// mark_walls_modified().
    void mark_modified_by_neighbor() noexcept;
    void mark_modified() noexcept;

    bool is_passability_modified() const noexcept;
    bool is_scenery_modified() const noexcept;
    bool are_walls_modified() const noexcept;

    using RTree = ::RTree<object_id, float, 2, float>;

    void ensure_alloc_ground();
    void ensure_alloc_walls();
    void ensure_ground_mesh(SpriteBatch& sb);
    ground_atlas* ground_atlas_at(usize i) const noexcept;
    void ensure_wall_mesh(SpriteBatch& sb);
    void ensure_scenery_mesh(SpriteBatch& sb, bool render_vobjs);

    void ensure_passability() noexcept;
    u64 pass_gen() const noexcept;
    /// @return The world::next_pass_gen() stamp of the last mark_objects_modified(), or 0 before the first.
    u64 objects_gen() const noexcept;
    /// @return The stamp of the last mark_modified_by_neighbor() or world::collect() deleting a chunk within 2,
    /// or 0. Only world::max_pass_gen() reads it.
    u64 neighbor_pass_gen() const noexcept;
    RTree* rtree() noexcept;
    const RTree* rtree() const noexcept;
    RTree* hole_rtree() noexcept;
    const RTree* hole_rtree() const noexcept;
    class world& world() noexcept;
    const class world& world() const noexcept;

    [[nodiscard]] bool can_place_object(const object_proto& proto, local_coords pos);
    // Creates no chunk. With none at coord, only its neighbours' colliders count.
    [[nodiscard]] static bool can_place_object(class world& w, chunk_coords_ coord, const object_proto& proto,
                                               local_coords pos);
    [[nodiscard]] static Optional<Pair<Range2D, pass_mode>> find_hole_in_bbox(const Chunk_RTree& rtree, Range2D bbox, pass_mode min_pass);
    using hole_callback = const function_view<void(Math::Range2D<float> hole, Math::Range1D<u8> z) const>;
    static void get_all_holes_in_bbox(const hole_callback& fn, chunk& c, Vector2 bb_min, Vector2 bb_max, pass_through_mask mask);

    void on_teardown();
    bool is_teardown() const;
    const object_storage& objects() const;
    object_storage& objects();

    void remove_object(const object& e, usize i);
    // unlike remove_object(), tears the script down and deletes the object
    void kill_object(const object& e, usize i, script_destroy_reason r = script_destroy_reason::kill);
    void sort_objects();

    struct ground_stuff
    {
        std::array<bptr<ground_atlas>, TILE_COUNT> atlases;
        std::array<variant_t, TILE_COUNT> variants = {};
    };

    struct wall_stuff
    {
        std::array<bptr<wall_atlas>, 2*TILE_COUNT> atlases;
        std::array<variant_t, 2*TILE_COUNT> variants;
    };

private:
    SpriteList scenery_static_mesh;
    SpriteList wall_static_mesh;
    SpriteList ground_static_mesh;
    Pointer<ground_stuff> _ground;
    Pointer<wall_stuff> _walls;
    object_storage _objects;
    class world* _world;
    Pointer<RTree> _rtree, _hole_rtree;
    Pointer<Grid::detail::grid_slots> _grid_slots;
    chunk* _next = nullptr;
    chunk* _prev = nullptr;
    chunk_coords_ _coord;
    u64 _pass_gen, _objects_gen = 0, _neighbor_pass_gen = 0;

    bool _maybe_empty      : 1 = true,
         _ground_modified  : 1 = true,
         _walls_modified   : 1 = true,
         _scenery_modified : 1 = true,
         _pass_modified    : 1 = true,
         _teardown         : 1 = false;

    void add_object(const bptr<object>& e);
    void add_object_pre(const bptr<object>& e);
    [[nodiscard]] usize add_objectʹ(const bptr<object>& e);
    void add_object_unsorted(const bptr<object>& e);
    void add_scenery_mesh(const object& e, SpriteBatch& sb, bool is_dynamic, i32 depth_offset);

    struct bbox final
    {
        collision_data data;
        Range2Di pos;

        bool operator==(const bbox& other) const noexcept;
    };

    [[nodiscard]] static bool _bbox_for_scenery(const object& s, bbox& value) noexcept;
    [[nodiscard]] static bool _bbox_for_scenery(const object& s, local_coords local, Vector2b offset,
                                                Vector2b bbox_offset, Vector2ub bbox_size, bbox& value) noexcept;

    static void _rtree_insert(RTree& t, const bbox& x);
    static void _rtree_remove(RTree& t, const bbox& x);
    static void _rtree_move(RTree& t, const bbox& x0, const bbox& x1);

    void _remove_bbox_dynamic(const bbox& x);
    void _remove_bbox_static(const bptr<object>& e, const bbox& x);
    void _remove_bbox_static_(const bptr<object>& e);

    void _add_bbox_dynamic(const bbox& x);
    void _add_bbox_static(const bptr<object>& e, const bbox& x);
    void _add_bbox_static_(const bptr<object>& e);

    template<bool Dynamic> void _replace_bbox_impl(const bptr<object>& e, const bbox& x0, const bbox& x, bool b0, bool b);
    void _replace_bbox_(const bptr<object>& e, const bbox& x0, const bbox& x, bool b0, bool b, bool upd);
    void _replace_bbox_dynamic(const bbox& x0, const bbox& x, bool b0, bool b);
    void _replace_bbox_static(const bptr<object>& e, const bbox& x0, const bbox& x, bool b0, bool b);

};

} // namespace floormat
