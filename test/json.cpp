#include "app.hpp"
#include "serialize/tile.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/magnum-vector.hpp"
#include "serialize/json-helper.hpp"
#include "compat/assert.hpp"
#include "src/tile-atlas.hpp"
#include "src/tile.hpp"
#include "src/tile-iterator.hpp"
#include "src/chunk.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Path.h>

namespace floormat {

#if 0
static chunk make_test_chunk()
{
    auto metal1 = loader.tile_atlas("metal1", {2, 2}, pass_mode::pass),
         metal2 = loader.tile_atlas("metal2", {2, 2}, pass_mode::blocked),
         tiles = loader.tile_atlas("tiles", {8, 5}, pass_mode::pass);
    constexpr auto N = TILE_MAX_DIM;
    world w;
    chunk c{w, {}};
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
#endif

void test_app::test_json() // NOLINT(readability-convert-member-functions-to-static)
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    const auto output_dir = Path::join(loader.TEMP_PATH, "test/."_s);
    {
        auto atlas = loader.tile_atlas("metal1", {2, 2}, pass_mode::pass);
        json_helper::to_json(atlas, Path::join(output_dir, "atlas.json"));
    }
    {
        Magnum::Math::Vector<2, int> v2i_1{1, 2};
        Vector2i v2i_2{2, 3};
        json_helper::to_json(v2i_1, Path::join(output_dir, "vec2i_1.json"));
        json_helper::to_json(v2i_2, Path::join(output_dir, "vec2i_2.json"));
    }
    {
        volatile float zero = 0;
        Magnum::Math::Vector3 vec{0.f/zero, -1.f/zero, 123.f};
        json_helper::to_json(vec, Path::join(output_dir, "vec3_inf.json"));
    }
}

} // namespace floormat
