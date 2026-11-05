#include "app.hpp"
#include "src/chunk.hpp"
#include "src/ground-atlas.hpp"
#include "src/tile-image.hpp"
#include "compat/borrowed-ptr.inl"
#include "loader/loader.hpp"

namespace floormat {

void app::maybe_initialize_chunk_([[maybe_unused]] const chunk_coords_& pos, chunk& c)
{
    auto floor1 = loader.ground_atlas("floor-tiles");

    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { floor1, variant_t(k % floor1->num_tiles()) };
    c.mark_modified();
}

void app::maybe_initialize_chunk([[maybe_unused]] const chunk_coords_& pos, [[maybe_unused]] chunk& c)
{
    //maybe_initialize_chunk_(pos, c);
}

} // namespace floormat
