#pragma once
#include "src/object-id.hpp"
#include "src/pass-mode.hpp"
#include "compat/borrowed-ptr-fwd.hpp"

namespace floormat { class chunk; class world; struct critter; }

namespace floormat::Test {

struct pred_object_ids
{
    object_id door_id, critter_id;
};

void rebuild_passability(chunk& c);
// Returns a world of the 3x3 chunks around (0, 0, 0), each with ground on every tile.
world make_world_with_ground();
// px is relative to the center of chunk (0, 0, 0)'s first tile.
bptr<critter> add_critter_at(world& w, Vector2i px, u8 size, pass_mode pass);
// Returns the ids of a door and of one critter, after adding colliders to the 3x3 chunks around (0, 0, 0), many of
// them across chunk borders.
pred_object_ids add_colliders(world& w);

} // namespace floormat::Test
