#pragma once
#include "compat/nanosecond.hpp"

namespace floormat {

class world;
struct chunk_coords_;

// Advances w by one frame of length dt and updates each object of `chunks` once, chunks in the order given.
// A coordinate with no chunk is skipped.
void update_world(world& w, ArrayView<const chunk_coords_> chunks, Ns dt);

} // namespace floormat
