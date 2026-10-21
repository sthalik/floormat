#pragma once
#include "src/point.hpp"
#include "src/tile-defs.hpp"
#include "src/object-id.hpp"
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

#ifdef FM_NO_DEBUG3
constexpr inline uint32_t exit_cells = 80;
#else
constexpr inline uint32_t exit_cells = 24;
#endif

// Destroys every object but `keep` on tiles [0, 2*cells]² and the square outside the exit.
// `pool` must be the one Dijkstra reads for the walker's bbox.
route replace(world& w, chunk_coords_ origin, uint32_t cells, uint64_t seed,
              Grid::Pass::Pool& pool, object_id keep);

} // namespace floormat::mazes
