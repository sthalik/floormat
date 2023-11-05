#include "test/app.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "loader/loader.hpp"
#include <algorithm>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Utility/Path.h>

namespace floormat {

using nlohmann::json;

namespace {

[[nodiscard]] Pair<wall_atlas_def, json> test_atlas_header(StringView path, StringView filename)
{
    auto j = json_helper::from_json_(Path::join(path, filename));
    wall_atlas_def def;
    test::read_atlas_header(j, def);

    fm_assert(def.info.name == "foo"_s);
    fm_assert(def.info.depth == 42);
    fm_assert(def.dir_count == 2);

    constexpr auto none = (uint8_t)-1;
    enum : uint8_t { N, E, S, W, };

    fm_assert(def.dir_indexes[N] == 0 || def.dir_indexes[N] == 1);
    fm_assert(def.dir_indexes[E] == none);
    fm_assert(def.dir_indexes[S] == none);

    if (def.dir_indexes[N] == 0)
        fm_assert(def.dir_indexes[W] == 1);
    else if (def.dir_indexes[N] == 1)
        fm_assert(def.dir_indexes[W] == 0);
    else
        fm_assert(false);

    return {std::move(def), std::move(j)};
}

} // namespace

void test_app::test_wall_atlas()
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    const auto path = Path::join(loader.TEMP_PATH, "test/json"_s);
    fm_assert(Path::isDirectory(path));

    (void)test_atlas_header(path, "frame_direction-header.json"_s);
}



} // namespace floormat
