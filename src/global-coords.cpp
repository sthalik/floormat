#include "global-coords.hpp"

namespace floormat {

static_assert(sizeof(decltype(local_coords::x))*8 == 8);
static_assert(sizeof(decltype(chunk_coords::x))*8 == 16);
static_assert(std::is_same_v<decltype(local_coords::x), decltype(local_coords::y)>);
static_assert(std::is_same_v<decltype(chunk_coords::x), decltype(chunk_coords::y)>);

static_assert(std::is_same_v<decltype(chunk_coords::x), decltype(chunk_coords::y)>);

static_assert(global_coords{{-1, -1}, {2, 3}} == global_coords{((-1 + (1 << 15)) << 4) + 2, ((-1 + (1 << 15)) << 4) + 3});
static_assert(global_coords{15, 15}.chunk() == global_coords{}.chunk());
static_assert(global_coords{15, 16}.chunk() != global_coords{}.chunk());
static_assert(global_coords{(1 + (1<<15)) << 4 | 3, (2 + (1<<15)) << 4 | 4} == global_coords{{1, 2}, {3, 4}});

} // namespace floormat
