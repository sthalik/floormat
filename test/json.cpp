#include "app.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/magnum-vector.hpp"
#include "serialize/tile.hpp"
#include "serialize/json-helper.hpp"
#include "compat/assert.hpp"
#include "tile-atlas.hpp"
#include "loader.hpp"

namespace Magnum::Examples {
bool app::test_json() // NOLINT(readability-convert-member-functions-to-static)
{
    bool ret = true;
    using nlohmann::to_json;
    const std::filesystem::path output_dir = "../test/";
    {
        nlohmann::json j;
        auto atlas = loader.tile_atlas("share/game/images/metal1.tga", {2, 2});
        ret &= json_helper::to_json(atlas, output_dir/"atlas.json");
    }
    {
        Magnum::Math::Vector<2, int> v2i_1{1, 2};
        Vector2i v2i_2{2, 3};
        ret &= json_helper::to_json(v2i_1, output_dir/"vec2i_1.json");
        ret &= json_helper::to_json(v2i_2, output_dir/"vec2i_2.json");
    }
    return ret;
}

} // namespace Magnum::Examples
