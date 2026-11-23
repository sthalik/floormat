#pragma once
#include "global-coords.hpp"
#include "search-pred.hpp"
#include "compat/function-view.hpp"
#include <array>

namespace floormat {
class world;
struct object;
class chunk;
struct point;
} // namespace floormat

namespace floormat::Grid::Pass { struct Params; }

namespace floormat::Search {

bool is_passable_1(chunk& c, Vector2 min, Vector2 max, const pred& p = never_continue);
bool is_passable_1(const chunk& c, Vector2 min, Vector2 max, const pred& p = never_continue);

bool is_passable_(chunk* c0, const std::array<chunk*, 8>& neighbors, Vector2 min, Vector2 max, const pred& p = never_continue);
bool is_passable_(const chunk* c0, const std::array<const chunk*, 8>& neighbors, Vector2 min, Vector2 max,
                  const pred& p = never_continue);

bool is_passable_neighbors_(const std::array<chunk*, 8>& neighbors, Vector2 min, Vector2 max,
                            const pred& p = never_continue);
bool is_passable_neighbors_(const std::array<const chunk*, 8>& neighbors, Vector2 min, Vector2 max,
                            const pred& p = never_continue);

bool is_passable(world& w, chunk_coords_ ch, const Range2D& bb, const pred& p = never_continue);
bool is_passable(const world& w, chunk_coords_ ch, const Range2D& bb, const pred& p = never_continue);

bool visit_colliders(chunk* c0, const std::array<chunk*, 8>& neighbors, Vector2 min, Vector2 max,
                     const collider_visitor& f);
bool visit_colliders(const chunk* c0, const std::array<const chunk*, 8>& neighbors, Vector2 min, Vector2 max,
                     const collider_visitor& f);

// Returns the bit a built pass grid of `params` holds for pt's cell, with dynamic_object_pred given cleared the
// way an object grid of those params clears it. Needs clean R-trees in pt's chunk and its 8 neighbors.
bool is_passable_by_rtree(const world& w, const Grid::Pass::Params& params, point pt,
                          const pred* dynamic_object_pred);

} // namespace floormat::Search
