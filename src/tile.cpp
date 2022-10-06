#include "tile.hpp"
#include "tile-atlas.hpp"

namespace Magnum::Examples {

local_coords::local_coords(std::size_t x, std::size_t y) : x{(std::uint8_t)x}, y{(std::uint8_t)y}
{
    CORRADE_INTERNAL_ASSERT(x <= 0xff && y <= 0xff);
}

} // namespace Magnum::Examples
