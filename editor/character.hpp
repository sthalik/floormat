#pragma once
#include "src/global-coords.hpp"
#include "src/rotation.hpp"
#include <memory>

namespace floormat {

struct anim_atlas;
struct world;

struct character_wip final
{
    character_wip();
    ~character_wip();
    void tick(world& w, float dt, bool L, bool R, bool U, bool D);

    std::shared_ptr<anim_atlas> walk_anim;
    global_coords pos;
    std::size_t frame = 0;
    float delta = 0;
    Vector2 offset;
    rotation r = rotation::NE;

private:
    int allocate_frame_time(float dt);
    static Vector2 move_vec(int left_right, int top_bottom);
};

} // namespace floormat
