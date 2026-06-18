#pragma once

namespace floormat {

using variant_t = uint16_t;
constexpr inline uint32_t TILE_MAX_DIM = 16;
constexpr inline size_t TILE_COUNT = size_t{TILE_MAX_DIM}*size_t{TILE_MAX_DIM};
constexpr inline int32_t tile_size_xy = 64;
constexpr inline int32_t tile_size_z = 192;

constexpr inline int8_t  chunk_z_min = -1, chunk_z_max = 14;

constexpr inline uint32_t chunk_coord_bits = 16;
constexpr inline int32_t  chunk_xy_max  =  (1 << (chunk_coord_bits-1)) - 1; // 32767 = INT16_MAX
constexpr inline int32_t  chunk_xy_min  = -(1 << (chunk_coord_bits-1));     // -32768 = INT16_MIN
constexpr inline uint32_t chunk_xy_bias =  1u << (chunk_coord_bits-1);      // 32768, biases chunk x/y non-negative

constexpr inline int32_t most_positive_point_xy = (1 << (chunk_coord_bits-1)) * tile_size_xy * (int32_t)TILE_MAX_DIM - (tile_size_xy/2 + 1);
constexpr inline int32_t most_positive_point_z = tile_size_z * chunk_z_max;

constexpr inline int32_t most_negative_point_xy = -((1 << (chunk_coord_bits-1)) * tile_size_xy * (int32_t)TILE_MAX_DIM) - tile_size_xy/2;
constexpr inline int32_t most_negative_point_z  = tile_size_z * chunk_z_min;

} // namespace floormat
