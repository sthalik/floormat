#pragma once
#include "compat/defs.hpp"
#include "src/global-coords.hpp"
#include "src/rotation.hpp"
#include "src/pass-mode.hpp"
#include "src/object-type.hpp"
#include "src/object-id.hpp"
#include "src/point.hpp"
#include <memory>

namespace floormat {

template<typename T> struct object_type_;
class anim_atlas;
class world;
class chunk;
struct Ns;

struct object_proto
{
    std::shared_ptr<anim_atlas> atlas;
    Vector2b offset, bbox_offset;
    Vector2ub bbox_size = Vector2ub(tile_size_xy);
    uint32_t delta = 0;
    uint16_t frame = 0;
    object_type type : 3              = object_type::none;
    rotation r       : rotation_BITS  = rotation::N;
    pass_mode pass   : pass_mode_BITS = pass_mode::see_through; // todo move to struct scenery, add inherit bit

    object_proto& operator=(const object_proto&);
    object_proto();
    object_proto(const object_proto&);

    virtual bool operator==(const object_proto&) const;
    bool operator!=(const object_proto& o) const { return !operator==(o); }
    virtual ~object_proto() noexcept;

    object_type type_of() const noexcept;
};

struct object
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(object);

    const object_id id = 0;
    uint64_t last_frame_no = 0;
    class chunk* const c;
    const std::shared_ptr<anim_atlas> atlas;
    const global_coords coord;
    const Vector2b offset, bbox_offset;
    const Vector2ub bbox_size;
    uint32_t delta = 0;
    uint16_t frame = 0;
    const rotation r = rotation::N; // todo remove bitfield?
    const pass_mode pass = pass_mode::see_through;
    bool ephemeral : 1 = false;
    //char _pad[4]; // got 4 bytes left

    virtual ~object() noexcept;

    virtual Vector2 ordinal_offset(Vector2b offset) const = 0;
    virtual float depth_offset() const = 0;
    float ordinal() const;
    float ordinal(local_coords xy, Vector2b offset, Vector2s z_offset) const;
    class chunk& chunk() const;
    size_t index() const;
    virtual bool is_virtual() const;
    point position() const;

    explicit operator object_proto() const;

    virtual object_type type() const noexcept = 0;
    virtual bool can_activate(size_t i) const;
    virtual bool activate(size_t i);
    virtual void update(size_t& i, const Ns& dt) = 0;
    void rotate(size_t i, rotation r);
    bool can_rotate(global_coords coord, rotation new_r, rotation old_r, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size);
    bool can_move_to(Vector2i delta, global_coords coord, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_aize);
    void set_bbox(Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass);

    object_type type_of() const noexcept;
    static point normalize_coords(global_coords coord, Vector2b cur_offset, Vector2i delta);
    static point normalize_coords(const point& pt, Vector2i delta);

    virtual bool is_dynamic() const;
    bool can_rotate(rotation new_r);
    bool can_move_to(Vector2i delta);
    bool move_to(size_t& i, Vector2i delta, rotation new_r);
    bool move_to(Vector2i delta);
    void teleport_to(size_t& i, global_coords coord, Vector2b offset, rotation new_r);
    void teleport_to(size_t& i, point pt, rotation new_r);

    template<typename T>
    requires std::is_unsigned_v<T>
    static uint32_t alloc_frame_time(const Ns& dt, T& accum, uint32_t hz, float speed);

protected:
    object(object_id id, class chunk& c, const object_proto& proto);
    void set_bbox_(Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass);
};

} // namespace floormat
