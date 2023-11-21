#include "wall-atlas.hpp"
#include "magnum-vector2i.hpp"
#include "magnum-vector.hpp"
#include "corrade-string.hpp"
#include "compat/exception.hpp"
#include "loader/loader.hpp"
#include <utility>
#include <string_view>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StringStl.h>
#include <Magnum/ImageView.h>
#include <Magnum/Trade/ImageData.h>
#include <nlohmann/json.hpp>

// todo add test on dummy files that generates 100% coverage on the j.contains() blocks!

namespace floormat::Wall::detail {

uint8_t direction_index_from_name(StringView s)
{
    for (uint8_t i = 0; auto [n, _] : wall_atlas::directions)
        if (n == s)
            return i;
        else
            i++;

    fm_throw("bad rotation name '{}'"_cf, fmt::string_view{s.data(), s.size()});
}

StringView direction_index_to_name(size_t i)
{
    fm_soft_assert(i < arraySize(wall_atlas::directions));
    return wall_atlas::directions[i].name;
}

Array<Frame> read_all_frames(const json& jroot)
{
    fm_assert(jroot.contains("frames"));

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
        // todo check index within range
    }

    if (jgroup.contains("pixel-size"))
        val.pixel_size = jgroup["pixel-size"];
    if (jgroup.contains("tint-mult"))
        val.tint_mult = Vector4(jgroup["tint-mult"]);
    if (jgroup.contains("tint-add"))
        val.tint_add = Vector3(jgroup["tint-add"]);
    if (jgroup.contains("from-rotation") && !jgroup["from-rotation"].is_null())
        val.from_rotation = (uint8_t)direction_index_from_name(std::string{ jgroup["from-rotation"] });
    if (jgroup.contains("mirrored"))
        val.mirrored = !!jgroup["mirrored"];
    if (jgroup.contains("default-tint"))
        val.default_tint = !!jgroup["default-tint"];

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

    for (auto [s_, memfn, tag] : Direction::groups)
    {
        std::string_view s = {s_.data(), s_.size()};
        if (!jdir.contains(s))
            continue;
        val.*memfn = read_group_metadata(jdir[s]);
    }

    val.top.pixel_size = val.top.pixel_size.flipped();

    return val;
}

Pair<Array<Direction>, std::array<DirArrayIndex, 4>> read_all_directions(const json& jroot)
{
    size_t count = 0;
    for (auto [str, _] : wall_atlas::directions)
        if (jroot.contains(str))
            count++;
    Array<Direction> array{count};
    std::array<DirArrayIndex, 4> map = {};
    for (uint8_t i = 0; auto [str, dir] : wall_atlas::directions)
        if (jroot.contains(str))
        {
            map[(size_t)dir] = {.val = i};
            array[i++] = read_direction_metadata(jroot, dir);
        }
    return { std::move(array), std::move(map) };
}

Info read_info_header(const json& jroot)
{
    fm_soft_assert(jroot.contains(("name")));
    fm_soft_assert(jroot.contains(("depth")));
    Info val = {std::string{jroot["name"]}, {}, jroot["depth"]};
    fm_soft_assert(val.depth > 0);
    if (jroot.contains("description"))
        val.description = std::string{jroot["description"]};
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
    constexpr Group group_defaults;

    fm_assert(jgroup.is_null());

    if (val.index != (uint32_t)-1)
        jgroup["offset"] = val.index;
    else
        jgroup["offset"] = -1;

    jgroup["count"] = val.count;
    jgroup["pixel-size"] = val.pixel_size;
    jgroup["tint-mult"] = Vector4(val.tint_mult);
    jgroup["tint-add"] = Vector3(val.tint_add);
    if (val.from_rotation != group_defaults.from_rotation)
        jgroup["from-rotation"] = direction_index_to_name(val.from_rotation);
    else
        jgroup["from-rotation"] = nullptr;
    jgroup["mirrored"] = val.mirrored;
    jgroup["default-tint"] = val.default_tint;
}

void write_direction_metadata(json& jdir, const Direction& dir)
{
    for (auto [s_, memfn, tag] : Direction::groups)
    {
        std::string_view s = {s_.data(), s_.size()};
        const auto& group = dir.*memfn;
        write_group_metadata(jdir[s], group);
    }
    if (jdir.contains("top"))
    {
        auto& top = jdir["top"];
        if (top.contains("pixel-size"))
            top["pixel-size"] = Vector2i{top["pixel-size"]}.flipped();
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
{    jroot["name"] = info.name;
    if (info.description)
        jroot["description"] = info.description;
    jroot["depth"] = info.depth;
}

} // namespace floormat::Wall::detail

namespace floormat::Wall {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Frame, offset)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Info, name, description, depth)

} // namespace floormat::Wall

namespace nlohmann {

using namespace floormat;
using namespace floormat::Wall;
using namespace floormat::Wall::detail;

void adl_serializer<Frame>::to_json(nlohmann::json& j, const Frame& x) { using nlohmann::to_json; to_json(j, x); }
void adl_serializer<Frame>::from_json(const json& j, Frame& x) { using nlohmann::from_json; from_json(j, x); }

void adl_serializer<std::shared_ptr<wall_atlas>>::to_json(json& j, const std::shared_ptr<const wall_atlas>& x)
{
    fm_assert(x != nullptr);
    write_info_header(j, x->info());
    write_all_directions(j, *x);
    write_all_frames(j, x->raw_frame_array());
}

void adl_serializer<std::shared_ptr<wall_atlas>>::from_json(const json& j, std::shared_ptr<wall_atlas>& x)
{
    auto info = read_info_header(j);
    fm_assert(loader.check_atlas_name(info.name));
    auto [dirs, map] = read_all_directions(j);
    Array<Frame> frames;
    auto img = loader.texture(loader.WALL_TILESET_PATH, info.name);
    if (j.contains("frames"))
        frames = read_all_frames(j);

    x = std::make_shared<wall_atlas>(std::move(info), img, std::move(frames), std::move(dirs), map);
}

} // namespace nlohmann
