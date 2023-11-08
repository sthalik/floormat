#include "wall-atlas.hpp"
#include "magnum-vector2i.hpp"
#include "magnum-vector.hpp"
#include "compat/exception.hpp"
#include <utility>
#include <string_view>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringStlView.h>
#include <nlohmann/json.hpp>

// todo add test on dummy files that generates 100% coverage on the j.contains() blocks!

namespace floormat::Wall::detail {

using nlohmann::json;
using namespace std::string_literals;

namespace {

constexpr auto none = (uint8_t)-1;
constexpr StringView direction_names[] = { "n"_s, "e"_s, "s"_s, "w"_s, };

} // namespace

uint8_t direction_index_from_name(StringView s)
{
    for (uint8_t i = 0; auto n : direction_names)
        if (n == s)
            return i;
        else
            i++;

    fm_throw("bad rotation name '{}'"_cf, fmt::string_view{s.data(), s.size()});
}

StringView direction_index_to_name(size_t i)
{
    fm_soft_assert(i < arraySize(direction_names));
    return direction_names[i];
}

Group read_group_metadata(const json& jgroup)
{
    fm_assert(jgroup.is_object());

    Group val;

    {
        int count = 0, index = -1;
        bool has_count = jgroup.contains("count"s) && (count = jgroup["count"s]) != 0,
             has_index = jgroup.contains("index"s);
        fm_soft_assert(has_count == has_index);
        fm_soft_assert(!has_index || index >= 0 && index < 1 << 20);
        // todo check index within range;
    }

    if (jgroup.contains("pixel-size"s))
        val.pixel_size = jgroup["pixel-size"s];
    if (jgroup.contains("tint-mult"s))
        val.tint_mult = Vector4(jgroup["tint-mult"s]);
    if (jgroup.contains("tint-add"s))
        val.tint_add = Vector3(jgroup["tint-add"s]);
    if (jgroup.contains("from-rotation"s))
        val.from_rotation = (uint8_t)direction_index_from_name(std::string{ jgroup["from-rotation"s] });
    if (jgroup.contains("mirrored"s))
        val.mirrored = !!jgroup["mirrored"s];
    if (jgroup.contains("default-tint"s))
        val.default_tint = !!jgroup["default-tint"s];

    return val;
}

Direction read_direction_metadata(const json& jroot, Direction_ dir)
{
    std::string_view s = direction_index_to_name((size_t)dir);
    if (!jroot.contains(s))
        return {};
    const auto& jdir = jroot[s];

    Direction val;

    for (auto [s_, memfn, tag] : Direction::members)
    {
        std::string_view s = s_;
        if (!jdir.contains(s))
            continue;
        val.*memfn = read_group_metadata(jdir[s]);
    }

    return val;
}

Info read_info_header(const json& jroot)
{
    fm_soft_assert(jroot.contains(("name"s)));
    fm_soft_assert(jroot.contains(("depth")));
    Info val = {std::string{jroot["name"s]}, {}, jroot["depth"s]};
    fm_soft_assert(val.depth > 0);
    if (jroot.contains("description"s))
        val.description = std::string{jroot["description"s]};
    return val;
}

void write_direction_metadata(json& jroot, const Direction& dir, Direction_ i)
{
    auto name = std::string_view{direction_index_to_name((size_t)i)};
    auto j = json{json::value_t::object};
    fm_assert(!jroot.contains(name));

    for (auto [s_, memfn, tag] : Direction::members)
    {
        std::string_view s = s_;
        const auto& group = dir.*memfn;
        write_group_metadata(j[s], group);
    }

    jroot[name] = std::move(j);
}

void write_group_metadata(json& jgroup, const Group& val)
{
    fm_assert(jgroup.is_object());
    fm_assert(jgroup.empty());

    jgroup["index"s] = val.index;
    jgroup["count"s] = val.count;
    jgroup["pixel-size"s] = val.pixel_size;
    jgroup["tint-mult"s] = Vector4(val.tint_mult);
    jgroup["tint-add"s] = Vector3(val.tint_add);
    jgroup["from-rotation"s] = val.from_rotation;
    jgroup["mirrored"s] = val.mirrored;
    jgroup["default-tint"s] = val.default_tint;
}

void write_info_header(json& jroot, const Info& info)
{
    jroot["name"s] = info.name;
    if (info.description)
        jroot["description"s] = info.description;
    jroot["depth"s] = info.depth;
}

} // namespace floormat::Wall::detail

namespace nlohmann {

using namespace floormat;
using namespace floormat::Wall::detail;

void adl_serializer<std::shared_ptr<wall_atlas>>::to_json(json& j, const std::shared_ptr<const wall_atlas>& x)
{

}

void adl_serializer<std::shared_ptr<wall_atlas>>::from_json(const json& j, std::shared_ptr<wall_atlas>& x)
{

}

} // namespace nlohmann
