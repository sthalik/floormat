#include "global-coords.hpp"

namespace floormat {

static_assert(sizeof(decltype(local_coords::x))*8 == 8);
static_assert(sizeof(decltype(chunk_coords::x))*8 == 16);
static_assert(std::is_same_v<decltype(local_coords::x), decltype(local_coords::y)>);
static_assert(std::is_same_v<decltype(chunk_coords::x), decltype(chunk_coords::y)>);

static_assert(std::is_same_v<decltype(chunk_coords::x), decltype(chunk_coords::y)>);

static_assert(TILE_MAX_DIM == (1 << 4));

static_assert(global_coords{(int)TILE_MAX_DIM-1, (int)TILE_MAX_DIM-1}.chunk() == global_coords{}.chunk());
static_assert(global_coords{(int)TILE_MAX_DIM-1, (int)TILE_MAX_DIM}.chunk() != global_coords{}.chunk());
static_assert(global_coords{(1u + (1<<15)) << 4 | 3, (2u + (1<<15)) << 4 | 4} == global_coords{{1, 2}, {3, 4}, -8});

static_assert(global_coords{-123, 456, 1}.z() == 1);
static_assert(global_coords{-123, 511, 5}.chunk() == chunk_coords{-8, 31});

} // namespace floormat
