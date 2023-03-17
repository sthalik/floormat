#pragma once
#include "compat/defs.hpp"
#include "src/global-coords.hpp"
#include "src/rotation.hpp"
#include "src/pass-mode.hpp"
#include "src/entity-type.hpp"
#include <memory>
#include <vector>

namespace floormat {

template<typename T> struct entity_type_;
struct anim_atlas;
struct world;
struct chunk;

struct entity_proto
{
    std::shared_ptr<anim_atlas> atlas;
    Vector2b offset, bbox_offset;
    Vector2ub bbox_size = Vector2ub(iTILE_SIZE2);
    std::uint16_t delta = 0, frame = 0;
    entity_type type : 3              = entity_type::none;
    rotation r       : rotation_BITS  = rotation::N;
    pass_mode pass   : pass_mode_BITS = pass_mode::see_through;

    float ordinal(local_coords coord) const;
    entity_proto& operator=(const entity_proto&);
    entity_proto();
    entity_proto(const entity_proto&);

    virtual bool operator==(const entity_proto&) const;
    virtual ~entity_proto() noexcept;
};

struct entity
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(entity);

    const std::uint64_t id = 0;
    struct chunk* const c;
    std::shared_ptr<anim_atlas> atlas;
    global_coords coord;
    const Vector2b offset, bbox_offset;
    const Vector2ub bbox_size;
    std::uint16_t delta = 0, frame = 0;
    const entity_type type;
    const rotation r = rotation::N;
    const pass_mode pass = pass_mode::see_through;

    virtual ~entity() noexcept;

    static Vector2b ordinal_offset_for_type(entity_type type, Vector2b offset);
    float ordinal() const;
    static float ordinal(local_coords xy, Vector2b offset, entity_type type);
    struct chunk& chunk() const;
    std::size_t index() const;

    explicit operator entity_proto() const;

    virtual bool can_activate(std::size_t i) const;
    virtual bool activate(std::size_t i);
    virtual bool update(std::size_t i, float dt) = 0;
    virtual void rotate(std::size_t i, rotation r);

    static Pair<global_coords, Vector2b> normalize_coords(global_coords coord, Vector2b cur_offset, Vector2i delta);
    [[nodiscard]] virtual bool can_move_to(Vector2i delta);
    std::size_t move(std::size_t i, Vector2i delta, rotation new_r);
    virtual void set_bbox(Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass);
    bool is_dynamic() const;

    friend struct world;

protected:
    entity(std::uint64_t id, struct chunk& c, entity_type type, const entity_proto& proto) noexcept;
    void set_bbox_(Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass);
};

} // namespace floormat
