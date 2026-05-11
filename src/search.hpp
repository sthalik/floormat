#pragma once
#include "global-coords.hpp"
#include "search-pred.hpp"
#include <array>

namespace floormat {
class world;
struct object;
class chunk;
} // namespace floormat

namespace floormat::Search {

bool is_passable_1(chunk& c, Vector2 min, Vector2 max, const Pred<chunk>& p = never_continue<chunk>());
bool is_passable_1(const chunk& c, Vector2 min, Vector2 max,
                   const Pred<const chunk>& p = never_continue<const chunk>());

bool is_passable_(chunk* c0, const std::array<chunk*, 8>& neighbors, Vector2 min, Vector2 max, const Pred<chunk>& p = never_continue<chunk>());
bool is_passable_(const chunk* c0, const std::array<const chunk*, 8>& neighbors, Vector2 min, Vector2 max,
                  const Pred<const chunk>& p = never_continue<const chunk>());

bool is_passable(world& w, chunk_coords_ ch, const Range2D& bb, const Pred<chunk>& p =  never_continue<chunk>());
bool is_passable(const world& w, chunk_coords_ ch, const Range2D& bb,
                 const Pred<const chunk>& p = never_continue<const chunk>());

} // namespace floormat::Search
