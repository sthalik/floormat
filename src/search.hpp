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

using Search::pred;

bool is_passable_1(chunk& c, Vector2 min, Vector2 max, const pred& p = Search::never_continue());
bool is_passable_(chunk* c0, const std::array<chunk*, 8>& neighbors,
                  Vector2 min, Vector2 max, const pred& p = Search::never_continue());
bool is_passable(world& w, chunk_coords_ ch0, const Range2D& bb, const pred& p = Search::never_continue());

bool is_passable_1(const chunk& c, Vector2 min, Vector2 max, const const_pred& p = const_never_continue());
bool is_passable_(const chunk* c0, const std::array<const chunk*, 8>& neighbors,
                  Vector2 min, Vector2 max, const const_pred& p = const_never_continue());
bool is_passable(const world& w, chunk_coords_ ch0, const Range2D& bb, const const_pred& p = const_never_continue());

} // namespace floormat::Search
