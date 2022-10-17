#include "chunk.hpp"

namespace floormat {

bool chunk::empty() const
{
    for (const tile& x : _tiles)
        if (x.ground_image || x.wall_north || x.wall_west)
            return false;

    return true;
}

} // namespace floormat
