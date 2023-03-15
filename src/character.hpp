#pragma once
#include "src/global-coords.hpp"
#include "src/rotation.hpp"
#include "src/entity.hpp"
#include <memory>

namespace floormat {

struct anim_atlas;
struct world;

struct character final : entity
{
    ~character() override;
    void set_keys(bool L, bool R, bool U, bool D);
    bool update(It it, struct chunk& c, float dt) override;

private:
    int allocate_frame_time(float dt);
    static Vector2 move_vec(int left_right, int top_bottom);

    friend struct world;
    character(std::uint64_t id, struct chunk& c, entity_type type);

    Vector2s offset_frac;
    bool b_L : 1 = false, b_R : 1 = false, b_U : 1 = false, b_D : 1 = false;
};

template<> struct entity_type_<struct character> : std::integral_constant<entity_type, entity_type::character> {};

} // namespace floormat
