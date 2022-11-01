#include "chunk.hpp"

namespace floormat {

bool chunk::empty(bool force) const noexcept
{
    if (!force && !_maybe_empty)
        return false;

    for (std::size_t i = 0; i < TILE_COUNT; i++)
    {
        if (_ground_atlases[i] || _wall_north_atlases[i] || _wall_west_atlases[i])
        {
            _maybe_empty = false;
            return false;
        }
    }

    return true;
}

} // namespace floormat
