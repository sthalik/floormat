#pragma once

namespace floormat {

using variant_t = u16;
constexpr inline u32 TILE_MAX_DIM = 16;
constexpr inline usize TILE_COUNT = usize{TILE_MAX_DIM}*usize{TILE_MAX_DIM};

// a Vector3 would get the xy size in z too
template<typename T> concept scalar_or_vector2 = !requires { T::Size; } || T::Size == 2;
template<scalar_or_vector2 T> constexpr inline T tile_size{64};
template<scalar_or_vector2 T> constexpr inline T half_tile{tile_size<i32>/2};
template<scalar_or_vector2 T> constexpr inline T chunk_size{tile_size<i32> * (i32)TILE_MAX_DIM};

// T is a Range. Holds every collider rect of an object in the chunk:
// offset + bbox_offset ± bbox_size/2 ends less than 0x100 past its tile.
template<typename T> constexpr inline T chunk_collision_bounds{
    -half_tile<typename T::VectorType> - typename T::VectorType{0x100},
    chunk_size<typename T::VectorType> - half_tile<typename T::VectorType> + typename T::VectorType{0x100},
};

constexpr inline i32 tile_size_xy = tile_size<i32>;
constexpr inline i32 tile_size_z = 192;
constexpr inline u32 chunk_size_xy = chunk_size<u32>;
static_assert((chunk_size_xy & (chunk_size_xy - 1)) == 0, "chunk_size_xy must be a power of two");

constexpr inline i8  chunk_z_min = -1, chunk_z_max = 14;
constexpr inline i32 chunk_z_count = chunk_z_max - chunk_z_min + 1;

constexpr inline u32 chunk_coord_bits = 16;
constexpr inline i32  chunk_xy_max  =  (1 << (chunk_coord_bits-1)) - 1; // 32767 = INT16_MAX
constexpr inline i32  chunk_xy_min  = -(1 << (chunk_coord_bits-1));     // -32768 = INT16_MIN
constexpr inline u32 chunk_xy_bias =  1u << (chunk_coord_bits-1);      // 32768, biases chunk x/y non-negative

constexpr inline i32 most_positive_point_xy = (1 << (chunk_coord_bits-1)) * chunk_size<i32> - (half_tile<i32> + 1);
constexpr inline i32 most_positive_point_z = tile_size_z * chunk_z_max;

constexpr inline i32 most_negative_point_xy = -((1 << (chunk_coord_bits-1)) * chunk_size<i32>) - half_tile<i32>;
constexpr inline i32 most_negative_point_z  = tile_size_z * chunk_z_min;

} // namespace floormat
