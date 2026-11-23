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
// Builds the grids that w.grid_orders() names and runs its cover orders' rebuilds and octant fills, as jobs of w's
// thread pool if it has one, then empties it. Needs w.grid_registry() unless there are none.
void run_grid_orders(world& w);
// Calls world::ensure_passability(), then builds the stale grids that readers requested and runs w.grid_orders()
// as run_grid_orders() does, so scripts may think. Needs w.grid_registry() unless there are no grid orders.
void prepare_world_for_thinking(world& w);

} // namespace floormat
