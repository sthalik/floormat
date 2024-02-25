#include "wall-atlas.hpp"
#include "magnum-vector.hpp"
#include "corrade-string.hpp"
#include "compat/exception.hpp"
#include "loader/loader.hpp"
#include "pass-mode.hpp"
#include "json-helper.hpp"
#include "corrade-array.hpp"
#include <utility>
#include <string_view>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StringStl.h>
#include <Magnum/ImageView.h>
#include <Magnum/Trade/ImageData.h>
#include <nlohmann/json.hpp>

namespace floormat::Wall {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Frame, offset, size)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Info, name, depth)
} // namespace floormat::Wall

namespace nlohmann {

using floormat::Wall::Frame;

template<>
struct adl_serializer<floormat::Wall::Frame> {
    static void to_json(json& j, const Frame& val);
    static void from_json(const json& j, Frame& val);
};

void adl_serializer<Frame>::to_json(json& j, const Frame& x) { using nlohmann::to_json; to_json(j, x); }
void adl_serializer<Frame>::from_json(const json& j, Frame& x) { using nlohmann::from_json; from_json(j, x); }

} // namespace nlohmann

namespace floormat {

using namespace floormat::Wall;
using namespace floormat::Wall::detail;

namespace {

struct direction_triple
{
    Array<Direction> array;
    std::array<DirArrayIndex, Direction_COUNT> map;
    std::array<bool, Direction_COUNT> mask;
};

direction_triple read_all_directions(const json& jroot)
{
    size_t count = 0;
    for (auto [str, _] : wall_atlas::directions)
        if (jroot.contains(str))
            count++;
    direction_triple ret = { Array<Direction>{count}, {}, {}, };
    auto& [array, map, mask] = ret;
    for (uint8_t i = 0, pos = 0; i < std::size(wall_atlas::directions); i++)
    {
        auto [str, dir] = wall_atlas::directions[i];
        if (jroot.contains(str))
        {
            mask[i] = true;
            map[i] = {.val = pos};
            array[pos++] = read_direction_metadata(jroot, dir);
        }
    }
    return ret;
}

} // namespace

bool wall_atlas_def::operator==(const wall_atlas_def& other) const noexcept
{
    if (header != other.header)
        return false;
    if (direction_array.size() != other.direction_array.size())
        return false;
    for (uint8_t i = 0; i < std::size(direction_map); i++)
    {
        auto i1 = direction_map[i], i2 = other.direction_map[i];
        if (!i1 != !i2)
            return false;
        if (i1)
        {
            fm_assert(i1.val < direction_array.size());
            fm_assert(i2.val < other.direction_array.size());
            if (direction_array[i1.val] != other.direction_array[i2.val])
                return false;
        }
    }
    if (frames.size() != other.frames.size())
        return false;
    for (auto i = 0uz; i < frames.size(); i++)
        if (frames[i] != other.frames[i])
            return false;
    return true;
}

wall_atlas_def wall_atlas_def::deserialize(StringView filename)
{
    wall_atlas_def atlas;

    const auto jroot = json_helper::from_json_(filename);
    atlas.header = read_info_header(jroot);
    fm_soft_assert(loader.check_atlas_name(atlas.header.name));
    atlas.frames = read_all_frames(jroot);
    auto [dirs, dir_indexes, mask] = read_all_directions(jroot);
    fm_soft_assert(!dirs.isEmpty());
    fm_soft_assert(dir_indexes != std::array<Wall::DirArrayIndex, Direction_COUNT>{});
    atlas.direction_array = std::move(dirs);
    atlas.direction_map = dir_indexes;
    atlas.direction_mask = mask;

    return atlas;
}

void wall_atlas_def::serialize(StringView filename, const Info& header,
                               ArrayView<const Frame> frames,
                               ArrayView<const Direction> dir_array,
                               std::array<DirArrayIndex, Direction_COUNT> dir_map)
{
    auto jroot = json{};

    write_info_header(jroot, header);
    write_all_frames(jroot, frames);

    for (const auto [name_, dir] : wall_atlas::directions)
        if (auto idx = dir_map[(size_t)dir])
        {
            const auto& dir = dir_array[idx.val];
            if (is_direction_defined(dir))
            {
                std::string_view name = {name_.data(), name_.size()};
                write_direction_metadata(jroot[name], dir);
            }
        }

    json_helper::to_json_(jroot, filename);
}

void wall_atlas_def::serialize(StringView filename) const
{
    serialize(filename, header, frames, direction_array, direction_map);
}

void wall_atlas::serialize(StringView filename) const
{
    return wall_atlas_def::serialize(filename, _info, _frame_array, _dir_array, _direction_map);
}

} // namespace floormat

namespace floormat::Wall::detail {

Array<Frame> read_all_frames(const json& jroot)
{
    if (!jroot.contains("frames"))
        return {};

    Array<Frame> frames;
    const auto& jframes = jroot["frames"];

    fm_assert(jframes.is_array());
    const auto sz = jframes.size();
    frames = Array<Frame>{sz};

    for (auto i = 0uz; i < sz; i++)
    {
        const auto& jframe = jframes[i];
        fm_assert(jframe.is_object());
        frames[i] = jframe;
    }

    return frames;
}

bool is_direction_defined(const Direction& dir)
{
    for (auto [str, ptr, tag] : Direction::groups)
    {
        const auto& group = dir.*ptr;
        if (group.is_defined)
            return true;
    }
    return false;
}

// todo add test on dummy files that generates 100% coverage on the j.contains() blocks!

Group read_group_metadata(const json& jgroup)
{
    fm_assert(jgroup.is_object());
    Group val;

    {
        int count = 0, index = -1;
        bool has_count = jgroup.contains("count") && (count = jgroup["count"]) != 0,
             has_index = jgroup.contains("offset") && (index = jgroup["offset"]) != -1;
        fm_soft_assert(has_count == has_index);
        fm_soft_assert(!has_index || index >= 0 && index < 1 << 20);
        fm_soft_assert(count >= 0);
        if (has_count)
        {
            val.index = (uint32_t)index;
            val.count = (uint32_t)count;
        }
    }

    val.default_tint = true;
    if (jgroup.contains("tint"))
    {
        val.tint_mult = jgroup["tint"];
        val.default_tint = false;
    }

    if (jgroup.contains("pixel-size"))
        val.pixel_size = jgroup["pixel-size"];
    if (jgroup.contains("mirrored"))
        val.mirrored = !!jgroup["mirrored"];
    if (jgroup.contains("from-rotation") && !jgroup["from-rotation"].is_null())
        val.from_rotation = direction_index_from_name(jgroup["from-rotation"]);

    val.is_defined = true;
    return val;
}

Direction read_direction_metadata(const json& jroot, Direction_ dir)
{
    const auto s_ = direction_index_to_name((size_t)dir);
    fm_assert(s_.size() == 1);
    std::string_view s = {s_.data(), s_.size()};
    fm_assert(s.size() == 1);
    if (!jroot.contains(s))
        return {};
    const auto& jdir = jroot[s];

    Direction val;

    for (auto [name, memfn, tag] : Direction::groups)
    {
        std::string_view s = {name.data(), name.size()};
        if (!jdir.contains(s))
            continue;
        val.*memfn = read_group_metadata(jdir[s]);
    }

    val.top.pixel_size = val.top.pixel_size.flipped();

    return val;
}

Info read_info_header(const json& jroot)
{
    fm_soft_assert(jroot.contains(("name")));
    fm_soft_assert(jroot.contains(("depth")));
    Info val = {std::string{jroot["name"]}, jroot["depth"]};
    fm_soft_assert(val.depth > 0);
    if (jroot.contains("pass-mode"))
        val.passability = jroot["pass-mode"];
    return val;
}

void write_all_frames(json& jroot, ArrayView<const Frame> array)
{
    auto jframes = json{};
    for (const Frame& frame : array)
    {
        json jframe = frame;
        jframes.push_back(std::move(jframe));
    }
    jroot["frames"] = std::move(jframes);
}

void write_group_metadata(json& jgroup, const Group& val)
{
    fm_assert(jgroup.is_null());
    fm_assert(val.is_defined);

    if (val.index != (uint32_t)-1)
        jgroup["offset"] = val.index;
    else
        jgroup["offset"] = -1;

    jgroup["count"] = val.count;
    jgroup["pixel-size"] = val.pixel_size;
    if (!val.default_tint)
        jgroup["tint"] = val.tint_mult;
    jgroup["mirrored"] = val.mirrored;
    if (val.from_rotation != (uint8_t )-1)
        jgroup["from-rotation"] = direction_index_to_name(val.from_rotation);
}

void write_direction_metadata(json& jdir, const Direction& dir)
{
    for (auto [name, memfn, tag] : Direction::groups)
    {
        const auto& group = dir.*memfn;
        if (!group.is_defined)
            continue;
        std::string_view s = {name.data(), name.size()};
        write_group_metadata(jdir[s], group);
    }
    if (jdir.contains("top"))
    {
        json& top = jdir["top"];
        Vector2i vec = top["pixel-size"];
        top["pixel-size"] = vec.flipped();
    }
}

void write_all_directions(json& jroot, const wall_atlas& a)
{
    for (auto [name, i] : wall_atlas::directions)
    {
        if (const auto* dir = a.direction((size_t)i))
        {
            auto jdir = json{};
            write_direction_metadata(jdir, *dir);
            jroot[name] = std::move(jdir);
        }
    }
}

void write_info_header(json& jroot, const Info& info)
{
    jroot["name"] = info.name;
    jroot["depth"] = info.depth;
    jroot["pass-mode"] = info.passability;
}

} // namespace floormat::Wall::detail
