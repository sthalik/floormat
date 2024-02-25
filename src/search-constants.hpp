#pragma once
#include "tile-constants.hpp"

namespace floormat::Search {

constexpr inline int div_factor = 4;
constexpr inline auto div_size = iTILE_SIZE2 / div_factor;
constexpr inline auto min_size = Vector2ui(div_size * 2);
constexpr inline auto div_count = iTILE_SIZE2 * TILE_MAX_DIM / div_size;

} // namespace floormat::Search
