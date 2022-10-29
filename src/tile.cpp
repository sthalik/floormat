#include "tile.hpp"

namespace floormat {

bool operator==(const tile& a, const tile& b) noexcept
{
    return a.ground      == b.ground &&
           a.wall_north  == b.wall_north &&
           a.wall_west   == b.wall_west &&
           a.passability == b.passability;
}

} // namespace floormat
