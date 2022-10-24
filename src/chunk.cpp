#include "chunk.hpp"

namespace floormat {

bool chunk::empty(bool force) const noexcept
{
    if (!force && !_maybe_empty)
        return false;

    for (const tile& x : _tiles)
        if (x.ground_image || x.wall_north || x.wall_west)
        {
            _maybe_empty = false;
            return false;
        }

    return true;
}

} // namespace floormat
