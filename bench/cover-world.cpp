#include "cover-world.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/scenery-proto.hpp"
#include "src/tile-image.hpp"
#include "src/wall-atlas.hpp"
#include "src/tile-defs.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "compat/borrowed-ptr.inl"

namespace floormat {

namespace {

void make_cover_chunk(world& w, chunk& c)
{
    const auto floor = tile_image_proto{ loader.ground_atlas("tiles"), 0 };
    const auto wall = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    const auto ch = c.coord();

    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
        {
            c[{i, j}].ground() = floor;
            if (j == 5 && i != 7 && i != 8)
                c[{i, j}].wall_north() = wall;
            if (i == 10 && j != 3 && j != 11)
                c[{i, j}].wall_west() = wall;
            if ((i*7 + j*3) % 4 == 0)
            {
                scenery_proto p;
                p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
                p.subtype   = generic_scenery_proto{};
                p.bbox_size = {24, 24};
                p.pass      = pass_mode::blocked;
                w.make_scenery(w.make_id(), {ch, {i, j}}, move(p));
            }
            else if ((i*5 + j) % 23 == 0)
                (void)w.make_object<critter>(w.make_id(), {ch, {i, j}}, critter_proto{});
        }
}

} // namespace

void make_cover_world(world& w)
{
    for_cover_chunks([&](chunk_coords_ ch) { make_cover_chunk(w, w[ch]); });
    for_cover_chunks([&](chunk_coords_ ch) {
        auto& c = w[ch];
        c.mark_passability_modified();
        c.ensure_passability();
    });
}

} // namespace floormat
