#pragma once
#include "compat/defs.hpp"
#include "src/global-coords.hpp"
#include "src/rotation.hpp"
#include "src/pass-mode.hpp"
#include "src/object-type.hpp"
#include "src/object-id.hpp"
#include <memory>
#include <vector>

namespace floormat {

template<typename T> struct object_type_;
struct anim_atlas;
struct world;
struct chunk;

struct object_proto
{
    std::shared_ptr<anim_atlas> atlas;
    Vector2b offset, bbox_offset;
    Vector2ub bbox_size = Vector2ub(iTILE_SIZE2);
    uint16_t delta = 0, frame = 0;
    object_type type : 3              = object_type::none;
    rotation r       : rotation_BITS  = rotation::N;
    pass_mode pass   : pass_mode_BITS = pass_mode::see_through;

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
    struct chunk* const c;
    const std::shared_ptr<anim_atlas> atlas;
    const global_coords coord;
    const Vector2b offset, bbox_offset;
    const Vector2ub bbox_size;
    uint16_t delta = 0, frame = 0;
    const rotation r = rotation::N;
    const pass_mode pass = pass_mode::see_through;

    virtual ~object() noexcept;

    virtual Vector2 ordinal_offset(Vector2b offset) const = 0;
    virtual float depth_offset() const = 0;
    float ordinal() const;
    float ordinal(local_coords xy, Vector2b offset, Vector2s z_offset) const;
    struct chunk& chunk() const;
    size_t index() const;
    virtual bool is_virtual() const;

    explicit operator object_proto() const;

    virtual object_type type() const noexcept = 0;
    virtual bool can_activate(size_t i) const;
    virtual bool activate(size_t i);
    virtual void update(size_t i, float dt) = 0;
    virtual void rotate(size_t i, rotation r);
    virtual bool can_rotate(global_coords coord, rotation new_r, rotation old_r, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size);
    virtual bool can_move_to(Vector2i delta, global_coords coord, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_aize);
    virtual void set_bbox(Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass);

    object_type type_of() const noexcept;
    static Pair<global_coords, Vector2b> normalize_coords(global_coords coord, Vector2b cur_offset, Vector2i delta);

    virtual bool is_dynamic() const;
    bool can_rotate(rotation new_r);
    bool can_move_to(Vector2i delta);
    void move_to(size_t& i, Vector2i delta, rotation new_r);
    void move_to(Vector2i delta);

protected:
    object(object_id id, struct chunk& c, const object_proto& proto);
    void set_bbox_(Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass);
};

} // namespace floormat