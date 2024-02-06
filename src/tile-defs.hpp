#pragma once

namespace floormat {

using variant_t = uint16_t;
constexpr inline uint32_t TILE_MAX_DIM = 16;
constexpr inline size_t TILE_COUNT = size_t{TILE_MAX_DIM}*size_t{TILE_MAX_DIM};
constexpr inline int32_t tile_size_xy = 64;
constexpr inline int32_t tile_size_z = 192;

} // namespace floormat
