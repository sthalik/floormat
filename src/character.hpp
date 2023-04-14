#pragma once
#include "src/global-coords.hpp"
#include "src/rotation.hpp"
#include "src/entity.hpp"
#include <Corrade/Containers/String.h>

namespace floormat {

struct anim_atlas;
struct world;

struct character_proto : entity_proto
{
    String name;
    bool playable : 1 = false;

    character_proto();
    character_proto(const character_proto&);
    ~character_proto() noexcept override;
    character_proto& operator=(const character_proto&);
    bool operator==(const entity_proto& proto) const override;
};

struct character final : entity
{
    entity_type type() const noexcept override;
    explicit operator character_proto() const;

    bool update(size_t i, float dt) override;
    void set_keys(bool L, bool R, bool U, bool D);
    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;

    String name;
    Vector2s offset_frac;
    bool b_L : 1 = false, b_R : 1 = false, b_U : 1 = false, b_D : 1 = false;
    bool playable : 1 = false;

private:
    int allocate_frame_time(float dt);
    static Vector2 move_vec(Vector2i vec);

    friend struct world;
    character(object_id id, struct chunk& c, const character_proto& proto);
};

template<> struct entity_type_<struct character> : std::integral_constant<entity_type, entity_type::character> {};
template<> struct entity_type_<struct character_proto> : std::integral_constant<entity_type, entity_type::character> {};

} // namespace floormat
