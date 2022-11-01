#include "app.hpp"
#include "src/world.hpp"
#include "src/loader.hpp"
#include "src/tile-atlas.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat {

namespace Path = Corrade::Utility::Path;

static chunk make_test_chunk()
{
    auto metal1 = loader.tile_atlas("metal1", {2, 2}),
         metal2 = loader.tile_atlas("metal2", {2, 2}),
         tiles = loader.tile_atlas("tiles", {8, 5});
    constexpr auto N = TILE_MAX_DIM;
    chunk c;
    for (auto [x, k, pt] : c) {
        x.ground() = { tiles, variant_t(k % tiles->num_tiles()) };
    }
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north() = { metal1, 0 };
    c[{K,   K  }].wall_west()  = { metal2, 0 };
    c[{K,   K+1}].wall_north() = { metal1, 0 };
    c[{K+1, K  }].wall_west()  = { metal2, 0 };
    return c;
}

static bool chunks_equal(const chunk& a, const chunk& b)
{
    for (std::size_t i = 0; i < TILE_COUNT; i++)
        if (a[i] != b[i])
            return false;
    return true;
}

static bool test_serializer1()
{
    constexpr auto filename = "../test/test-serializer1.dat";
    if (Path::exists(filename))
        Path::remove(filename);
    world w;
    const chunk_coords coord{1, 1};
    w[coord] = make_test_chunk();
    w.serialize(filename);
    auto w2 = world::deserialize(filename);
    return chunks_equal(w[coord], w2[coord]);
}

bool floormat::test_serializer()
{
    bool ret = true;
    ret &= test_serializer1();
    return ret;
}

} // namespace floormat
