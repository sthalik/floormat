#include "tile.hpp"
#include <limits>

namespace Magnum::Examples {

chunk::tile_index_array_type chunk::make_tile_indices() noexcept
{
    tile_index_array_type array;
    for (unsigned i = 0; i < N*N; i++)
        array[i] = (std::uint8_t)i;
    return array;
}

world::world() = default;

} // namespace Magnum::Examples
