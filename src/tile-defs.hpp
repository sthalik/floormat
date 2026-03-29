#pragma once

namespace floormat {

using variant_t = uint16_t;
constexpr inline uint32_t TILE_MAX_DIM = 16;
constexpr inline size_t TILE_COUNT = size_t{TILE_MAX_DIM}*size_t{TILE_MAX_DIM};
constexpr inline int32_t tile_size_xy = 64;
constexpr inline int32_t tile_size_z = 192;

constexpr inline int8_t  chunk_z_min = -1, chunk_z_max = 14;

constexpr inline int32_t most_positive_point_xy = 1 << 24;
constexpr inline int32_t most_positive_point_z = tile_size_z * chunk_z_max;

constexpr inline int32_t most_negative_point_xy = -most_positive_point_xy;
constexpr inline int32_t most_negative_point_z  = tile_size_z * chunk_z_min;

} // namespace floormat
