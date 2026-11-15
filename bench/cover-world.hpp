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

// Rays from chunk (0, 0) reach loaded neighbours, as in the game. A ray leaving into a missing
// chunk hits nothing and runs to the 1024 px cap.
void make_cover_world(world& w);

} // namespace floormat
