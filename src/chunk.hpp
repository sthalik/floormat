#pragma once
#include "object-id.hpp"
#include "tile.hpp"
#include "local-coords.hpp"
#include "src/RTree-fwd.h"
#include "global-coords.hpp"
#include "search-pred.hpp"
#include "sprite-list.hpp"
#include <array>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Pointer.h>

namespace floormat::Quads { using indexes = std::array<UnsignedShort, 6>; }

namespace floormat {

class anim_atlas;
class wall_atlas;
struct object;
struct object_proto;
class SpriteBatch;
struct tile_shader;
struct clickable;

class chunk final
{
public:
    friend struct tile_ref;
    friend struct object;
    friend class world;

    tile_ref operator[](size_t idx) noexcept;
    tile_proto operator[](size_t idx) const noexcept;
    tile_ref operator[](local_coords xy) noexcept;
    tile_proto operator[](local_coords xy) const noexcept;

    chunk_coords_ coord() const noexcept;
    Optional<tile_ref> at_offset(local_coords pos, Vector2i off);
    Optional<tile_ref> at_offset(tile_ref r, Vector2i off);

    bool empty(bool force = false) const noexcept;

    explicit chunk(class world& w, chunk_coords_ ch) noexcept;
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
    bool are_walls_modified() const noexcept;

    struct pass_region;

    using RTree = ::RTree<object_id, float, 2, float>;

    void ensure_alloc_ground();
    void ensure_alloc_walls();
    void ensure_ground_mesh(SpriteBatch& sb);
    ground_atlas* ground_atlas_at(size_t i) const noexcept;
    void ensure_wall_mesh(SpriteBatch& sb);

    SpriteList scenery_static_mesh;
    SpriteList wall_static_mesh;
    SpriteList ground_static_mesh;

    void ensure_scenery_mesh(SpriteBatch& sb, bool render_vobjs);
    void add_clickables(const tile_shader& shader, Vector2i win_size, Array<clickable>& array, bool draw_vobjs);

    void ensure_passability() noexcept;
    RTree* rtree() noexcept;
    class world& world() noexcept;

    [[nodiscard]] bool can_place_object(const object_proto& proto, local_coords pos);
    [[nodiscard]] static bool find_hole_in_bbox(Range2D& hole, const Chunk_RTree& rtree, Vector2 min, Vector2 max);
    using hole_callback = const fu2::function_view<void(Math::Range2D<float> hole, Math::Range1D<uint8_t> z) const>;
    static void get_all_holes_in_bbox(const hole_callback& fn, chunk& c, Vector2 bb_min, Vector2 bb_max);

    void on_teardown();
    bool is_teardown() const;
    ArrayView<const bptr<object>> objects() const;

    void remove_object(size_t i);
    void sort_objects();

    pass_region make_pass_region(bool debug = false, ArrayView<const Vector2i> positions = {});
    pass_region make_pass_region(const Search::pred& f, bool debug = false, ArrayView<const Vector2i> positions = {});

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
    Pointer<ground_stuff> _ground;
    Pointer<wall_stuff> _walls;
    Array<bptr<object>> _objects;
    class world* _world;
    Pointer<RTree> _rtree;
    chunk_coords_ _coord;

    mutable bool _maybe_empty      : 1 = true,
                 _ground_modified  : 1 = true,
                 _walls_modified   : 1 = true,
                 _scenery_modified : 1 = true,
                 _pass_modified    : 1 = true,
                 _teardown         : 1 = false,
                 _objects_sorted   : 1 = true;

    void add_object(const bptr<object>& e);
    void add_object_pre(const bptr<object>& e);
    [[nodiscard]] size_t add_objectʹ(const bptr<object>& e);
    void add_object_unsorted(const bptr<object>& e);

    struct bbox final
    {
        collision_data data;
        Vector2i start, end;

        bool operator==(const bbox& other) const noexcept;
    };

    [[nodiscard]] static bool _bbox_for_scenery(const object& s, bbox& value) noexcept;
    [[nodiscard]] static bool _bbox_for_scenery(const object& s, local_coords local, Vector2b offset,
                                                Vector2b bbox_offset, Vector2ub bbox_size, bbox& value) noexcept;

    void _remove_bbox_(const bptr<object>& e, const bbox& x, bool upd, bool is_dynamic);
    void _remove_bbox_dynamic(const bbox& x);
    void _remove_bbox_static(const bptr<object>& e, const bbox& x);
    void _remove_bbox_static_(const bptr<object>& e);

    void _add_bbox_(const bptr<object>& e, const bbox& x, bool upd, bool is_dynamic);
    void _add_bbox_dynamic(const bbox& x);
    void _add_bbox_static(const bptr<object>& e, const bbox& x);
    void _add_bbox_static_(const bptr<object>& e);

    template<bool Dynamic> void _replace_bbox_impl(const bptr<object>& e, const bbox& x0, const bbox& x, bool b0, bool b);
    void _replace_bbox_(const bptr<object>& e, const bbox& x0, const bbox& x, bool b0, bool b, bool upd, bool is_dynamic);
    void _replace_bbox_dynamic(const bbox& x0, const bbox& x, bool b0, bool b);
    void _replace_bbox_static(const bptr<object>& e, const bbox& x0, const bbox& x, bool b0, bool b);

};

} // namespace floormat
