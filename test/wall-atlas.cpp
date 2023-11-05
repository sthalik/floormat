#include "test/app.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "loader/loader.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat::Wall::detail {

using nlohmann::json;
using namespace std::string_literals;

namespace {

StringView json_path()
{
    static const auto path = [] {
        fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
        const auto path = Path::join(loader.TEMP_PATH, "test/json"_s);
        fm_assert(Path::isDirectory(path));
        return path;
    }();
    return path;
}

void test_read_info(StringView filename)
{
    const auto jroot = json_helper::from_json_(Path::join(json_path(), filename));
    fm_assert(jroot.contains("directions"s));
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

void test_read_empty_direction(StringView filename)
{
    const auto jroot = json_helper::from_json_(Path::join(json_path(), filename));
    test_read_info(filename);
    fm_assert(!jroot.empty());
    fm_assert(jroot.contains("directions"s));
    const auto& jdir = jroot["directions"s];

    fm_assert( jdir.contains("n"s) );
    fm_assert(!jdir.contains("e"s) );
    fm_assert(!jdir.contains("s"s) );
    fm_assert( jdir.contains("w"s) );

    {   auto g = read_group_metadata(jdir["w"]);
        fm_assert(g.is_empty());
    }
}

} // namespace

} // namespace floormat::Wall::detail

void floormat::test_app::test_wall_atlas()
{
    using namespace floormat::Wall::detail;
    test_read_info("wall-atlas-header1.json"_s);
    test_read_empty_direction("wall-atlas-header1.json"_s);
}
