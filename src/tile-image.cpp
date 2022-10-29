#include "tile-image.hpp"

namespace floormat {

bool operator==(const tile_image& a, const tile_image& b) noexcept
{
    return a.atlas == b.atlas && a.variant == b.variant;
}

} // namespace floormat

