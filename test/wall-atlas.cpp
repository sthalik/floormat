#include "test/app.hpp"
#include "src/wall-atlas.hpp"
#include "serialize/wall-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "loader/loader.hpp"
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

void test_read_empty_directions(StringView filename)
{
    const auto jroot = json_helper::from_json_(Path::join(json_path(), filename));
    test_read_header(filename);
    fm_assert(!jroot.empty());

    fm_assert( jroot.contains("n") );
    fm_assert(!jroot.contains("e") );
    fm_assert(!jroot.contains("s") );
    fm_assert( jroot.contains("w") );

    fm_assert(jroot["n"].is_object() && jroot["n"].empty());
    fm_assert(jroot["w"].is_object() && jroot["w"].empty());
}

void test_read_groups(StringView filename)
{
    constexpr Group group_defaults;
    const auto jroot = json_helper::from_json_(Path::join(json_path(), filename));

    auto info = read_info_header(jroot);
    fm_assert(info.name == "foo"_s);
    fm_assert(info.depth == 42);
    fm_assert(info.description == ""_s);

    fm_assert(jroot["depth"] == 42);
    fm_assert( jroot.contains("n") );
    fm_assert(!jroot.contains("e") );
    fm_assert(!jroot.contains("s") );
    fm_assert( jroot.contains("w") );
    fm_assert(jroot["n"].is_object() && jroot["n"].empty());
    fm_assert(jroot["w"].is_object() && !jroot["w"].empty());
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
    bool operator==(const wall_atlas_&) const noexcept;

    Info header;
    std::array<Direction, 4> directions = {};
    Array<Frame> frames;
};

[[nodiscard]] wall_atlas_ read_from_file(StringView filename, bool do_checks = true)
{
    wall_atlas_ atlas;

    const auto jroot = json_helper::from_json_(filename);
    atlas.header = read_info_header(jroot);

    if (do_checks)
    {
        const Info header_defaults;
        fm_assert(atlas.header.name != header_defaults.name);
        fm_assert(atlas.header.depth != header_defaults.depth);
    }

    bool got_any_directions = false;
    for (const auto& [_, curdir] : wall_atlas::directions)
    {
        auto i = (size_t)curdir;
        atlas.directions[i] = read_direction_metadata(jroot, curdir);
        got_any_directions = got_any_directions || atlas.directions[i];
    }
    if (do_checks)
        fm_assert(got_any_directions);

    atlas.frames = read_all_frames(jroot);

    if (do_checks)
    {
        constexpr Frame frame_defaults;
        constexpr Group group_defaults;

        fm_assert(!atlas.frames.isEmpty());
        fm_assert(atlas.frames[0].offset != frame_defaults.offset);
        fm_assert(atlas.directions[(size_t)Direction_::W].side.pixel_size != group_defaults.pixel_size);
    }

    return atlas;
}

void write_to_temp_file(const wall_atlas_& atlas)
{
    auto jroot = json{};

    write_info_header(jroot, atlas.header);
    write_all_frames(jroot, atlas.frames);

    for (const auto [name_, dir] : wall_atlas::directions)
    {
        std::string_view name = {name_.data(), name_.size()};
        auto i = (size_t)dir;
        if (atlas.directions[i])
            write_direction_metadata(jroot[name], atlas.directions[i]);
    }

    const auto filename = temp_filename();
    json_helper::to_json_(jroot, filename);
}

bool wall_atlas_::operator==(const wall_atlas_& other) const noexcept
{
    if (header != other.header)
        return false;
    if (directions != other.directions)
        return false;
    if (frames.size() != other.frames.size())
        return false;
    for (auto i = 0uz; i < frames.size(); i++)
        if (frames[i] != other.frames[i])
            return false;
    return true;
}

} // namespace

} // namespace floormat::Wall::detail

void floormat::test_app::test_wall_atlas()
{
    using namespace floormat::Wall::detail;
    constexpr auto S_01_header_json = "wall-atlas-01_header.json"_s,
                   S_02_groups_json = "wall-atlas-02_groups.json"_s;

    { test_read_header(S_01_header_json);
      test_read_empty_directions(S_01_header_json);
    }

    { test_read_header(S_02_groups_json);
      test_read_groups(S_02_groups_json);
    }

    { auto a = read_from_file(Path::join(json_path(), S_02_groups_json));
      write_to_temp_file(a);
      auto b = read_from_file(temp_filename());
      fm_assert(a == b);
    }
}
