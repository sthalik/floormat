#include "test/app.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "loader/loader.hpp"
#include <algorithm>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Utility/Path.h>

namespace floormat::Wall::detail {

using nlohmann::json;

namespace {

void test_atlas_header(StringView path, StringView filename)
{
    auto jroot = json_helper::from_json_(Path::join(path, filename));
    auto info = read_info_header(jroot);

    fm_assert(info.name == "foo"_s);
    fm_assert(info.depth == 42);

#if 0
    fm_assert(def.dir_indexes[N] == 0 || def.dir_indexes[N] == 1);
    fm_assert(def.dir_indexes[E] == none);
    fm_assert(def.dir_indexes[S] == none);

    if (def.dir_indexes[N] == 0)
        fm_assert(def.dir_indexes[W] == 1);
    else if (def.dir_indexes[N] == 1)
        fm_assert(def.dir_indexes[W] == 0);
    else
        fm_assert(false);
#endif
}

} // namespace

} // namespace floormat::Wall::detail

void floormat::test_app::test_wall_atlas()
{
    using namespace floormat::Wall::detail;

    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    const auto path = Path::join(loader.TEMP_PATH, "test/json"_s);
    fm_assert(Path::isDirectory(path));

    (void)test_atlas_header(path, "wall-atlas-header1.json"_s);
}
