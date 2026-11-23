#pragma once
#include "compat/nanosecond.hpp"

namespace floormat {

class world;
struct chunk_coords_;
struct order;
struct critter;

struct thinking_critter { const struct critter* critter; u32 update_index; };

// Advances w by one frame of length dt. Every critter and door of `chunks` gives its orders, chunks in the order
// given, and then the orders run. A coordinate with no chunk is skipped.
void update_world(world& w, ArrayView<const chunk_coords_> chunks, Ns dt);
// Runs one order of the frame being updated: finds o.giver in w and makes the change o.kind names.
void run_order(world& w, const order& o, const Ns& dt);
// Builds the pass and object grids that w.grid_orders() names, then runs its cover orders in update_index order
// and empties it. Needs w.grid_registry() unless there are none.
void run_grid_orders(world& w);
// Does what must be done before any script thinks: world::ensure_passability(), then run_grid_orders(), whose
// GridRegistry::build_listed_grids() call also builds the stale grids that readers requested.
void prepare_world_for_thinking(world& w);

} // namespace floormat
