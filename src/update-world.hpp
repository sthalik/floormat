#pragma once
#include "compat/nanosecond.hpp"

namespace floormat {

class world;
struct chunk_coords_;
struct order;

// Advances w by one frame of length dt. Every critter and door of `chunks` gives its orders, chunks in the order
// given, and then the orders run. A coordinate with no chunk is skipped.
void update_world(world& w, ArrayView<const chunk_coords_> chunks, Ns dt);
// Runs one order of the frame being updated: finds o.giver in w and makes the change o.kind names.
void run_order(world& w, const order& o, const Ns& dt);

} // namespace floormat
