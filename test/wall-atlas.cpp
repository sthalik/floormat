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

void test_read_header(StringView filename)
{
    const auto jroot = json_helper::from_json_(Path::join(json_path(), filename));
    auto info = read_info_header(jroot);
    fm_assert(info.name == "foo"_s);
    fm_assert(info.depth == 42);
}

void test_read_empty_direction(StringView filename)
{
    const auto jroot = json_helper::from_json_(Path::join(json_path(), filename));
    test_read_header(filename);
    fm_assert(!jroot.empty());

    fm_assert( jroot.contains("n"s) );
    fm_assert(!jroot.contains("e"s) );
    fm_assert(!jroot.contains("s"s) );
    fm_assert( jroot.contains("w"s) );

    fm_assert(jroot["n"s].is_object() && jroot["n"s].empty());
    fm_assert(jroot["w"s].is_object() && jroot["w"s].empty());
}

void test_read_groups(StringView filename)
{
    constexpr Group group_defaults;
    const auto jroot = json_helper::from_json_(Path::join(json_path(), filename));
    read_info_header(jroot);

    fm_assert(jroot["depth"s] == 42);
    fm_assert( jroot.contains("n"s) );
    fm_assert(!jroot.contains("e"s) );
    fm_assert(!jroot.contains("s"s) );
    fm_assert( jroot.contains("w"s) );
    fm_assert(jroot["n"s].is_object() && jroot["n"s].empty());
    fm_assert(jroot["w"s].is_object() && !jroot["w"s].empty());
    fm_assert(read_direction_metadata(jroot, Direction_::N).is_empty());
    fm_assert(read_direction_metadata(jroot, Direction_::E).is_empty());
    fm_assert(read_direction_metadata(jroot, Direction_::S).is_empty());

    const auto dir = read_direction_metadata(jroot, Direction_::W);
    fm_assert(dir.wall.pixel_size    == Vector2ui{}                        );
    fm_assert(dir.wall.default_tint  == false                              );
    fm_assert(dir.wall.mirrored      == group_defaults.mirrored            );
    fm_assert(dir.wall.from_rotation == (uint8_t)-1                        );
    fm_assert(dir.side.pixel_size    == Vector2ui{42, 192}                 );
    fm_assert(dir.side.default_tint  == true                               );
    fm_assert(dir.top.default_tint   == group_defaults.default_tint        );
    fm_assert(dir.overlay.tint_mult  == Vector4{0.125f, 0.25f, 0.5f, 1.f } );
    fm_assert(dir.overlay.tint_add   == Vector3{1, 2, 3}                   );
    fm_assert(dir.overlay.mirrored   == true                               );
}

} // namespace

} // namespace floormat::Wall::detail

void floormat::test_app::test_wall_atlas()
{
    using namespace floormat::Wall::detail;
    test_read_header("wall-atlas-01_header.json"_s);
    test_read_empty_direction("wall-atlas-01_header.json"_s);
    test_read_groups("wall-atlas-02_groups.json"_s);
}
