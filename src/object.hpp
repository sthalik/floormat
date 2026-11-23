#pragma once
#include "compat/defs.hpp"
#include "compat/borrowed-ptr.h"
#include "compat/propagate-const.hpp"
#include "global-coords.hpp"
#include "rotation.hpp"
#include "pass-mode.hpp"
#include "object-type.hpp"
#include "scenery-type.hpp"
#include "object-id.hpp"
#include "point.hpp"
#include "script-enums.hpp"
#include "search-pred.hpp"

namespace floormat {

class anim_atlas;
class world;
class chunk;
struct Ns;

struct object_proto
{
    bptr<anim_atlas> atlas;
    Vector2b offset, bbox_offset;
    Vector2ub bbox_size = Vector2ub(tile_size_xy);
    u32 delta = 0;
    u16 frame = 0;
    object_type type : 3              = object_type::none;
    rotation r = rotation::N;
    pass_mode pass = pass_mode::see_through; // todo move to struct scenery, add inherit bit

    object_proto() noexcept;
    virtual ~object_proto() noexcept;
    virtual bool operator==(const object_proto&) const;
    bool operator!=(const object_proto& o) const noexcept;
    object_type type_of() const noexcept;

    object_proto& operator=(const object_proto&) noexcept;
    object_proto(const object_proto&) noexcept;
    object_proto& operator=(object_proto&&) noexcept;
    object_proto(object_proto&&) noexcept;
};

struct object : bptr_base
{
    fm_DISABLE_COPY(object);
    fm_DISABLE_MOVE(object);

    const object_id id = 0;
    u64 last_frame_no = 0;
protected:
    propagate_const<class chunk*> c;
    const bptr<object> parent;
public:
    const bptr<anim_atlas> atlas;
    const global_coords coord;
    const Vector2b offset, bbox_offset;
    const Vector2ub bbox_size;
    u16 frame = 0;
    u32 delta = 0;
    const rotation r = rotation::N; // todo remove bitfield?
    const pass_mode pass = pass_mode::see_through;
    bool ephemeral : 1 = false;

    virtual ~object() noexcept override;

    virtual i32 depth_offset() const = 0;
    const class chunk& chunk() const;
    const class world& world() const;
    class chunk& chunk();
    class world& world();
    usize index() const;
    usize index();
    bool is_virtual() const;
    point position() const;

    explicit operator object_proto() const;

    object_type type() const noexcept;
    virtual bool can_activate(usize i) const;
    virtual bool activate(usize i);
    virtual void update(const bptr<object>& self, usize& i, const Ns& dt) = 0;
    void rotate(usize i, rotation r);
    bool can_rotate(global_coords coord, rotation new_r, rotation old_r, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size);
    bool can_rotate(global_coords coord, rotation new_r, rotation old_r, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size,
                    const Search::pred& p);
    bool can_move_to(Vector2i delta, global_coords coord, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size);
    bool can_move_to(Vector2i delta, global_coords coord, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size,
                     const Search::pred& p);
    // The const overloads create no chunk and test a missing one's neighbors alone. They need clean R-trees.
    bool can_rotate(global_coords coord, rotation new_r, rotation old_r, Vector2b offset, Vector2b bbox_offset,
                    Vector2ub bbox_size) const;
    bool can_rotate(global_coords coord, rotation new_r, rotation old_r, Vector2b offset, Vector2b bbox_offset,
                    Vector2ub bbox_size, const Search::pred& p) const;
    bool can_move_to(Vector2i delta, global_coords coord, Vector2b offset, Vector2b bbox_offset,
                     Vector2ub bbox_size) const;
    bool can_move_to(Vector2i delta, global_coords coord, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size,
                     const Search::pred& p) const;
    void set_bbox(Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass);

    object_type type_of() const noexcept;

    bool is_dynamic() const;
    constexpr bool updates_passability() const;
    bool updates_walls() const;
    virtual void mark_modified_with_neighbors();

    bool can_rotate(rotation new_r);
    bool can_move_to(Vector2i delta);
    bool can_rotate(rotation new_r) const;
    bool can_move_to(Vector2i delta) const;
    bool move_to(usize& i, Vector2i delta, rotation new_r);
    bool move_to(Vector2i delta);
    void teleport_to(usize& i, global_coords coord, Vector2b offset, rotation new_r);
    void teleport_to(usize& i, point pt, rotation new_r);

    /// delta holds ns. Stored time saturates at 2^32 - 1 ns, 4.29 s.
    static u32 alloc_frame_time(const Ns& dt, u32& delta, Ns frame, float speed);
    static Ns alloc_ticks(const Ns& dt, u32 delta, Ns frame, float speed);
    /// At hz 0 longer than any time alloc_ticks() returns, so an fps 0 atlas never advances.
    static Ns frame_duration(u32 hz);
    static u32 ns_to_delta(const Ns& time);
    /// Saves before proto 31 stored delta as a fraction of a second, (u32)-1 being one second.
    static Ns fraction_to_ns(u32 fraction);

    virtual void init_script(const bptr<object>& ptr);
    virtual void destroy_script_pre(const bptr<object>& ptr, script_destroy_reason r);
    virtual void destroy_script_post();

protected:
    const object_type _type;
    // What the non-const index() last returned. The const index() only reads it.
    // An insert or erase in the chunk's list can make it stale, so index() tests it.
    u32 _index_hint = 0;
    // In scenery, it would make generic_scenery and door_scenery bigger on MSVC, which doesn't reuse a base's
    // tail padding.
    const scenery_type _scenery_type;

    object(object_id id, class chunk& c, const object_proto& proto, object_type type, enum scenery_type scenery_type);
    void set_bbox_(Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass);

    static void check_script_update_1(script_lifecycle state);
    [[nodiscard]] static bool check_script_update_2(script_lifecycle state);
};

constexpr bool object::updates_passability() const { return _type == object_type::hole; }

} // namespace floormat
