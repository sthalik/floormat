#pragma once
#include <cstddef>

namespace floormat {

constexpr inline std::size_t TILE_MAX_DIM = 16;
constexpr inline std::size_t TILE_COUNT = TILE_MAX_DIM*TILE_MAX_DIM;
constexpr inline float TILE_SIZE[3] = { 128, 128, 384 };
constexpr inline double dTILE_SIZE[3] = { 128, 128, 384 };

} // namespace floormat
