#pragma once

namespace floormat {

class world;

// Returns 21×21 chunks around the origin with a wall on about one tile edge in 32.
world make_dense_world();

} // namespace floormat
