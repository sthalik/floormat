#include "test/app.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "loader/loader.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat::Wall::detail {

using nlohmann::json;
using namespace std::string_view_literals;

namespace {

const StringView temp_filename() {
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    static const auto tmp_filename = Path::join(loader.TEMP_PATH, "test/test-wall-atlas.json"_s);
    return tmp_filename;
};

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

    fm_assert( jroot.contains("n"sv) );
    fm_assert(!jroot.contains("e"sv) );
    fm_assert(!jroot.contains("s"sv) );
    fm_assert( jroot.contains("w"sv) );

    fm_assert(jroot["n"sv].is_object() && jroot["n"sv].empty());
    fm_assert(jroot["w"sv].is_object() && jroot["w"sv].empty());
}

void test_read_groups(StringView filename)
{
    constexpr Group group_defaults;
    const auto jroot = json_helper::from_json_(Path::join(json_path(), filename));

    auto info = read_info_header(jroot);
    fm_assert(info.name == "foo"_s);
    fm_assert(info.depth == 42);
    fm_assert(info.description == ""_s);

    fm_assert(jroot["depth"sv] == 42);
    fm_assert( jroot.contains("n"sv) );
    fm_assert(!jroot.contains("e"sv) );
    fm_assert(!jroot.contains("s"sv) );
    fm_assert( jroot.contains("w"sv) );
    fm_assert(jroot["n"sv].is_object() && jroot["n"sv].empty());
    fm_assert(jroot["w"sv].is_object() && !jroot["w"sv].empty());
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

struct wall_atlas_
{
    bool operator==(const wall_atlas_&) const noexcept = default;

    Info header;
    Array<Direction> directions;
    Array<Frame> frames;
};

void write_to_temp_file()
{
    const auto filename = temp_filename();

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
