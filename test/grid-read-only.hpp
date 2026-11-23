#pragma once
#include "src/object-id.hpp"
#include "src/pass-mode.hpp"
#include "src/global-coords.hpp"
#include "compat/borrowed-ptr-fwd.hpp"

namespace floormat { class chunk; class world; struct critter; }
namespace floormat::Grid::Pass { class Pool; }
namespace floormat::Grid::Object { class Pool; }
namespace floormat::Grid::Cover { class Pool; }

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
void add_scenery(world& w, global_coords pos, Vector2b bbox_offset, Vector2ub bbox_size, pass_mode pass);
// Returns the ids of a door and of one critter, after adding colliders to the 3x3 chunks around (0, 0, 0), many of
// them across chunk borders.
pred_object_ids add_colliders(world& w);
// Appends what a read-only query must leave unchanged: each chunk's stamps, flags and built grids with their
// stamps and octants, each pool's free grid count and octant fills, and world::pass_gen().
void record_state(const world& w, ArrayView<const Grid::Pass::Pool* const> pass_pools,
                  ArrayView<const Grid::Object::Pool* const> object_pools,
                  ArrayView<const Grid::Cover::Pool* const> cover_pools, Array<u64>& out);

} // namespace floormat::Test
