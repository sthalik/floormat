#include "app.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "loader/scenery.hpp"
#include "src/tile-atlas.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat {

namespace Path = Corrade::Utility::Path;

chunk test_app::make_test_chunk()
{
    auto metal1 = loader.tile_atlas("metal1", {2, 2}, pass_mode::pass),
         metal2 = loader.tile_atlas("metal2", {2, 2}, pass_mode::blocked),
         tiles  = loader.tile_atlas("tiles", {8, 5}, pass_mode::pass);
    constexpr auto N = TILE_MAX_DIM;
    chunk c;
    for (auto [x, k, pt] : c)
        x.ground() = { tiles, variant_t(k % tiles->num_tiles()) };
    auto door = loader.scenery("door1"),
         table = loader.scenery("table1"),
         control_panel = loader.scenery("control panel (wall) 1");
    control_panel.frame.r = rotation::W;
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north() = { metal1, 0 };
    c[{K,   K  }].wall_west()  = { metal2, 0 };
    c[{K,   K+1}].wall_north() = { metal1, 0 };
    c[{K+1, K  }].wall_west()  = { metal2, 0 };
    c[{K+3, K+1}].scenery() = door;
    c[{ 3,   4 }].scenery() = table;
    c[{K,   K+1}].scenery() = control_panel;
    c.mark_modified();
    return c;
}

static bool chunks_equal(const chunk& a, const chunk& b)
{
    for (std::size_t i = 0; i < TILE_COUNT; i++)
    {
        const auto &a1 = a[i], &b1 = b[i];
        if (a1 != b1)
            return false;
    }
    return true;
}

void test_app::test_serializer()
{
    constexpr auto filename = "../test/test-serializer1.dat";
    if (Path::exists(filename))
        Path::remove(filename);
    world w;
    const chunk_coords coord{1, 1};
    w[coord] = make_test_chunk();
    w.serialize(filename);
    auto w2 = world::deserialize(filename);
    auto &c1 = w[coord], &c2 = w2[coord];
    fm_assert(chunks_equal(c1, c2));
}

} // namespace floormat
