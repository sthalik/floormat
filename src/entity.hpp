#pragma once
#include "compat/defs.hpp"
#include "src/global-coords.hpp"
#include "src/rotation.hpp"
#include "src/pass-mode.hpp"
#include <memory>
#include <vector>

namespace floormat {

template<typename T> struct entity_type_;
struct anim_atlas;
struct world;

enum class entity_type : std::uint8_t {
    none, character, scenery,
};

struct entity_proto
{
    std::shared_ptr<anim_atlas> atlas;
    Vector2b offset, bbox_offset;
    Vector2ub bbox_size = Vector2ub(iTILE_SIZE2);
    std::uint16_t delta = 0, frame = 0;
    entity_type type = entity_type::none;
    rotation r     : rotation_BITS  = rotation::N;
    pass_mode pass : pass_mode_BITS = pass_mode::see_through;

    std::uint32_t ordinal(local_coords coord) const;
    entity_proto& operator=(const entity_proto&);
    entity_proto();
    entity_proto(const entity_proto&);

    bool operator==(const entity_proto&) const;
    virtual ~entity_proto() noexcept;
};

struct entity
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(entity);
    using It = typename std::vector<std::shared_ptr<entity>>::const_iterator;

    const std::uint64_t id = 0;
    world& w;
    std::shared_ptr<anim_atlas> atlas;
    global_coords coord;
    Vector2b offset, bbox_offset;
    Vector2ub bbox_size;
    std::uint16_t delta = 0, frame = 0;
    const entity_type type;
    rotation r     : rotation_BITS  = rotation::N;
    pass_mode pass : pass_mode_BITS = pass_mode::see_through;

    virtual ~entity() noexcept;

    std::uint32_t ordinal() const;
    static std::uint32_t ordinal(local_coords xy, Vector2b offset);
    struct chunk& chunk() const;
    It iter() const;

    virtual bool operator==(const entity_proto& e0) const;
    operator entity_proto() const;

    virtual bool can_activate(It it, struct chunk& c) const;
    virtual bool activate(It it, struct chunk& c);
    virtual bool update(It it, struct chunk& c, float dt) = 0;
    virtual void rotate(It it, struct chunk& c, rotation r);

    static Pair<global_coords, Vector2b> normalize_coords(global_coords coord, Vector2b cur_offset, Vector2i delta);
    [[nodiscard]] virtual bool can_move_to(Vector2i delta, struct chunk& c);
    static void move(It it, Vector2i delta, struct chunk& c);

    friend struct world;

protected:
    entity(std::uint64_t id, struct world& w, entity_type type) noexcept;
    entity(std::uint64_t id, struct world& w, entity_type type, const entity_proto& proto) noexcept;
};

} // namespace floormat
