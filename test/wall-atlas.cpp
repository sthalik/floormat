#include "test/app.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "loader/loader.hpp"
#include "compat/exception.hpp"
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Utility/Path.h>

namespace floormat::Wall::detail {

using nlohmann::json;

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

void test_read_groups(StringView filename)
{
    const auto path = Path::join(json_path(), filename);
    const auto jroot = json_helper::from_json_(path);

    auto info = read_info_header(jroot);
    fm_assert(info.name == "foo"_s);
    fm_assert(info.depth == 42);
    fm_assert(info.passability == pass_mode::shoot_through);

    fm_assert(jroot["depth"] == 42);
    fm_assert( jroot.contains("n") );
    fm_assert( jroot.contains("w") );
    fm_assert(jroot["n"].is_object() && !jroot["n"].empty());
    fm_assert(jroot["w"].is_object() && !jroot["w"].empty());
    fm_assert(is_direction_defined(read_direction_metadata(jroot, Direction_::N)));

    const auto dir = read_direction_metadata(jroot, Direction_::W);
    fm_assert(is_direction_defined(dir));
    fm_assert(dir.wall.pixel_size   == Vector2ui{42, 192}                 );
    fm_assert(dir.wall.tint_mult    == Vector4{0.125f, 0.25f, 0.5f, 1.f } );
    fm_assert(dir.wall.tint_add     == Vector3{1, 2, 3}                   );
    fm_assert(dir.wall.mirrored     == true                               );
    fm_assert(dir.wall.default_tint == false                              );
    fm_assert(dir.side.pixel_size   == Vector2ui{42, 192}                 );
    fm_assert(dir.side.default_tint == true                               );
    fm_assert(dir.top.default_tint  == true                               );

    const auto atlas2 = wall_atlas_def::deserialize(path);
    fm_assert(atlas2.header == info);
    auto idx2 = atlas2.direction_map[(size_t)Direction_::W];
    fm_assert(idx2);
    const auto& dir2 = atlas2.direction_array[idx2.val];
    fm_assert(dir == dir2);
}

[[nodiscard]] wall_atlas_def read_and_check(StringView filename)
{
    auto atlas = wall_atlas_def::deserialize(filename);

    const Info header_defaults;
    fm_assert(atlas.header.name != header_defaults.name);
    fm_assert(atlas.header.depth != header_defaults.depth);

    constexpr Frame frame_defaults;
    constexpr Group group_defaults;

    fm_assert(!atlas.frames.empty());
    fm_assert(atlas.frames[0].offset != frame_defaults.offset);
    auto dir_index = atlas.direction_map[(size_t)Direction_::W];
    fm_assert(dir_index);
    const auto& dir = atlas.direction_array[dir_index.val];
    fm_assert(dir.side.pixel_size != group_defaults.pixel_size);

    return atlas;
}

void test_expected_size()
{
    fm_assert_equal(Vector2ui{64, 192}, wall_atlas::expected_size(42, Group_::wall));
    fm_assert_equal(Vector2ui{42, 192}, wall_atlas::expected_size(42, Group_::side));
    fm_assert_equal(Vector2ui{32, 192}, wall_atlas::expected_size(42, Group_::corner_L));
    fm_assert_equal(Vector2ui{32, 192}, wall_atlas::expected_size(42, Group_::corner_R));
    // swapped in atlas.json during reading and writing, rotated counter-clockwise in atlas image file
    fm_assert_equal(Vector2ui{42, 192}, wall_atlas::expected_size(42, Group_::top));
}

} // namespace

} // namespace floormat::Wall::detail

void floormat::test_app::test_wall_atlas()
{
    using namespace floormat::Wall::detail;

    {
        test_expected_size();
    }

    {
        constexpr auto S_01_header_json = "wall-atlas-01_header.json"_s,
                       S_02_groups_json = "wall-atlas-02_groups.json"_s;

        test_read_header(S_01_header_json);

        { test_read_header(S_02_groups_json);
          test_read_groups(S_02_groups_json);
        }

        { auto a = read_and_check(Path::join(json_path(), S_02_groups_json));
          a.serialize(temp_filename());
          auto b = read_and_check(temp_filename());
          fm_assert(a == b);
        }
    }
}
