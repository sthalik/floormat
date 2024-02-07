#include "app.hpp"
#include "serialize/tile.hpp"
#include "serialize/ground-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "serialize/magnum-vector.hpp"
#include "serialize/json-helper.hpp"
#include "compat/assert.hpp"
#include "src/ground-atlas.hpp"
#include "src/tile.hpp"
#include "src/tile-iterator.hpp"
#include "src/chunk.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "loader/wall-cell.hpp"
#include <memory>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Path.h>

namespace floormat {

void test_app::test_json() // NOLINT(readability-convert-member-functions-to-static)
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    const auto output_dir = Path::join(loader.TEMP_PATH, "test/."_s);
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

void test_app::test_json2()
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    const auto output_dir = Path::join(loader.TEMP_PATH, "test/."_s);
    auto atlas1 = loader.make_invalid_ground_atlas().atlas;
    json_helper::to_json(atlas1, Path::join(output_dir, "atlas1.json"));
    auto atlas2 = loader.make_invalid_wall_atlas().atlas;
    atlas2->serialize(Path::join(output_dir, "atlas2.json"));
}

void test_app::test_json3()
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    const auto output_dir = Path::join(loader.TEMP_PATH, "test/."_s);
    auto atlas3 = loader.ground_atlas("metal1");
    json_helper::to_json(atlas3, Path::join(output_dir, "atlas3.json"));
    auto atlas4 = loader.wall_atlas("empty");
    atlas4->serialize(Path::join(output_dir, "atlas4.json"));
}

} // namespace floormat
