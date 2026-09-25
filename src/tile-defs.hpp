#pragma once

namespace floormat {

using variant_t = uint16_t;
constexpr inline uint32_t TILE_MAX_DIM = 16;
constexpr inline size_t TILE_COUNT = size_t{TILE_MAX_DIM}*size_t{TILE_MAX_DIM};

// a Vector3 would get the xy size in z too
template<typename T> concept scalar_or_vector2 = !requires { T::Size; } || T::Size == 2;
template<scalar_or_vector2 T> constexpr inline T tile_size{64};
template<scalar_or_vector2 T> constexpr inline T half_tile{tile_size<int32_t>/2};
template<scalar_or_vector2 T> constexpr inline T chunk_size{tile_size<int32_t> * (int32_t)TILE_MAX_DIM};

// T is a Range. Holds every collider rect of an object in the chunk:
// offset + bbox_offset ± bbox_size/2 ends less than 0x100 past its tile.
template<typename T> constexpr inline T chunk_collision_bounds{
    -half_tile<typename T::VectorType> - typename T::VectorType{0x100},
    chunk_size<typename T::VectorType> - half_tile<typename T::VectorType> + typename T::VectorType{0x100},
};

constexpr inline int32_t tile_size_xy = tile_size<int32_t>;
constexpr inline int32_t tile_size_z = 192;
constexpr inline uint32_t chunk_size_xy = chunk_size<uint32_t>;

constexpr inline int8_t  chunk_z_min = -1, chunk_z_max = 14;

constexpr inline uint32_t chunk_coord_bits = 16;
constexpr inline int32_t  chunk_xy_max  =  (1 << (chunk_coord_bits-1)) - 1; // 32767 = INT16_MAX
constexpr inline int32_t  chunk_xy_min  = -(1 << (chunk_coord_bits-1));     // -32768 = INT16_MIN
constexpr inline uint32_t chunk_xy_bias =  1u << (chunk_coord_bits-1);      // 32768, biases chunk x/y non-negative

constexpr inline int32_t most_positive_point_xy = (1 << (chunk_coord_bits-1)) * chunk_size<int32_t> - (half_tile<int32_t> + 1);
constexpr inline int32_t most_positive_point_z = tile_size_z * chunk_z_max;

constexpr inline int32_t most_negative_point_xy = -((1 << (chunk_coord_bits-1)) * chunk_size<int32_t>) - half_tile<int32_t>;
constexpr inline int32_t most_negative_point_z  = tile_size_z * chunk_z_min;

} // namespace floormat
