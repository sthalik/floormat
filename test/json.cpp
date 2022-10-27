#include "app.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/magnum-vector.hpp"
#include "serialize/tile.hpp"
#include "serialize/json-helper.hpp"
#include "compat/assert.hpp"
#include "tile-atlas.hpp"
#include "tile.hpp"
#include "chunk.hpp"
#include "loader.hpp"

namespace floormat {

[[maybe_unused]]
static chunk make_test_chunk()
{
    auto metal1 = loader.tile_atlas("metal1", {2, 2}),
         metal2 = loader.tile_atlas("metal2", {2, 2}),
         tiles = loader.tile_atlas("tiles", {8, 5});
    constexpr auto N = TILE_MAX_DIM;
    chunk c;
    for (auto& [x, k, pt] : c) {
        x.ground_image = { tiles, k % tiles->num_tiles() };
    }
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north = { metal1, 0 };
    c[{K,   K  }].wall_west  = { metal2, 0 };
    c[{K,   K+1}].wall_north = { metal1, 0 };
    c[{K+1, K  }].wall_west  = { metal2, 0 };
    return c;
}

bool floormat::test_json() // NOLINT(readability-convert-member-functions-to-static)
{
    const std::filesystem::path output_dir = "../test/.";
    {
        auto atlas = loader.tile_atlas("metal1", {2, 2});
        json_helper::to_json(atlas, output_dir/"atlas.json");
    }
    {
        Magnum::Math::Vector<2, int> v2i_1{1, 2};
        Vector2i v2i_2{2, 3};
        json_helper::to_json(v2i_1, output_dir/"vec2i_1.json");
        json_helper::to_json(v2i_2, output_dir/"vec2i_2.json");
    }
    {
        volatile float zero = 0;
        Magnum::Math::Vector3 vec{0.f/zero, -1.f/zero, 123.f};
        json_helper::to_json(vec, output_dir/"vec3_inf.json");
    }

    return true;
}

} // namespace floormat
