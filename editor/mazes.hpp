#pragma once
#include "src/point.hpp"
#include "src/tile-defs.hpp"
#include <cr/StringView.h>

namespace floormat { class world; }
namespace floormat::Grid::Pass { class Pool; }

// Pathfinding test maps, sealed, walls on tile edges. Shared by floormat-benchmark and the editor's
// PGO driver; each runs its own searches.
namespace floormat::mazes {

enum class kind : uint8_t { rooms, braid, prim, COUNT };

constexpr inline uint8_t bbox_size = 32;
constexpr inline uint32_t map_tiles = 8*TILE_MAX_DIM;
// The last chunk holds only the east and south seal.
constexpr inline int16_t map_chunks = (int16_t)(map_tiles/TILE_MAX_DIM + 1);

struct route
{
    point from, to;
    uint32_t max_dist;
    uint32_t bfs_tiles;
};

StringView name(kind k);

// `pool` must be the one Dijkstra reads for bbox_size: the endpoints are picked by its cell bits.
route build(world& w, kind k, chunk_coords_ origin, Grid::Pass::Pool& pool);

} // namespace floormat::mazes
