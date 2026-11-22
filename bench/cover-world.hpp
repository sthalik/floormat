#pragma once
#include "src/global-coords.hpp"

namespace floormat {

class world;

template<typename F> void for_cover_chunks(F&& fun)
{
    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
            fun(chunk_coords_{x, y, 0});
}

// Chunk (0, 0)'s cover grid reads its neighbours' pass grids, as in the game. A missing chunk would read as
// open ground and cost less to fill.
void make_cover_world(world& w);

} // namespace floormat
