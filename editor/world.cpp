#include "app.hpp"
#include "src/world.hpp"
#include "src/hole.hpp"
#include "src/wall-atlas.hpp"
#include "src/ground-atlas.hpp"
#include "src/tile-image.hpp"
#include "compat/borrowed-ptr.inl"
#include "floormat/main.hpp"
#include "loader/loader.hpp"

namespace floormat {

void app::populate_dev_test_world_2()
{
    reset_world();
    auto& w = M->world();

    constexpr chunk_coords_ ch{0, 0, 0};
    auto& c = w[ch];
    c.mark_modified();

    auto wall = loader.wall_atlas("test1", loader_policy::warn);
    auto ground = loader.ground_atlas("tiles");

    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { ground, variant_t(k % ground->num_tiles()) };

    c[{6, 8}].wall_north() = { wall, 0 };
    c[{8, 8}].wall_north() = { wall, 0 };
    c[{10, 8}].wall_north() = { wall, 0 };
    c[{6, 10}].wall_west()  = { wall, 0 };
    c[{6, 12}].wall_west()  = { wall, 0 };

    auto place_hole = [&](local_coords at, Vector2b off, Vector2ub size) {
        hole_proto p;
        p.z_offset = 0;
        p.height = tile_size_z;
        p.bbox_size = size;
        p.bbox_offset = off;
        p.pass = pass_mode::pass;
        w.make_object<hole>(w.make_id(), {ch, at}, p);
    };

    place_hole({8, 8}, { tile_size_xy/4,  -tile_size_xy/2}, { tile_size_xy/2, tile_size_xy });
    place_hole({10, 8}, {-tile_size_xy/4, -tile_size_xy/2}, { tile_size_xy/2, tile_size_xy });
    place_hole({6, 10}, {-tile_size_xy/2,  tile_size_xy/4}, { tile_size_xy,   tile_size_xy/2 });
}

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
