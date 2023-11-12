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

using namespace std::string_view_literals;

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

Group read_group_metadata(const json& jgroup)
{
    fm_assert(jgroup.is_object());

    Group val;

    {
        int count = 0, index = -1;
        bool has_count = jgroup.contains("count"sv) && (count = jgroup["count"sv]) != 0,
             has_index = jgroup.contains("offset"sv);
        if (has_index)
            index = jgroup["offset"sv];
        fm_soft_assert(has_count == has_index);
        fm_soft_assert(!has_index || index >= 0 && index < 1 << 20);
        // todo check index within range
    }

    if (jgroup.contains("pixel-size"sv))
        val.pixel_size = jgroup["pixel-size"sv];
    if (jgroup.contains("tint-mult"sv))
        val.tint_mult = Vector4(jgroup["tint-mult"sv]);
    if (jgroup.contains("tint-add"sv))
        val.tint_add = Vector3(jgroup["tint-add"sv]);
    if (jgroup.contains("from-rotation"sv))
        val.from_rotation = (uint8_t)direction_index_from_name(std::string{ jgroup["from-rotation"sv] });
    if (jgroup.contains("mirrored"sv))
        val.mirrored = !!jgroup["mirrored"sv];
    if (jgroup.contains("default-tint"sv))
        val.default_tint = !!jgroup["default-tint"sv];

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

    val.top.pixel_size = val.top.pixel_size.flipped();

    return val;
}

Info read_info_header(const json& jroot)
{
    fm_soft_assert(jroot.contains(("name"sv)));
    fm_soft_assert(jroot.contains(("depth")));
    Info val = {std::string{jroot["name"sv]}, {}, jroot["depth"sv]};
    fm_soft_assert(val.depth > 0);
    if (jroot.contains("description"sv))
        val.description = std::string{jroot["description"sv]};
    return val;
}

void write_group_metadata(json& jgroup, const Group& val)
{
    fm_assert(jgroup.is_object());
    fm_assert(jgroup.empty());

    jgroup["offset"sv] = val.index;
    jgroup["count"sv] = val.count;
    jgroup["pixel-size"sv] = val.pixel_size;
    jgroup["tint-mult"sv] = Vector4(val.tint_mult);
    jgroup["tint-add"sv] = Vector3(val.tint_add);
    jgroup["from-rotation"sv] = val.from_rotation;
    jgroup["mirrored"sv] = val.mirrored;
    jgroup["default-tint"sv] = val.default_tint;
}

void write_direction_metadata(json& jdir, const Direction& dir)
{
    //auto name = std::string_view{direction_index_to_name((size_t)i)};

    for (auto [s_, memfn, tag] : Direction::members)
    {
        std::string_view s = s_;
        const auto& group = dir.*memfn;
        write_group_metadata(jdir[s], group);
    }
    if (jdir.contains("top"sv))
    {
        auto& top = jdir["top"sv];
        if (top.contains("pixel-size"sv))
            top["pixel-size"sv] = Vector2i{top["pixel-size"sv]}.flipped();
    }
}

void write_all_directions(json& jroot, const wall_atlas& a)
{
    for (auto [name, i] : wall_atlas::directions)
    {
        if (const auto* dir = a.direction((size_t)i))
        {
            auto jdir = json{json::value_t::object};
            write_direction_metadata(jdir, *dir);
            jroot[name] = jdir;
        }
    }
}

void write_info_header(json& jroot, const Info& info)
{
    jroot["name"sv] = info.name;
    if (info.description)
        jroot["description"sv] = info.description;
    jroot["depth"sv] = info.depth;
}

} // namespace floormat::Wall::detail

namespace nlohmann {

using namespace floormat;
using namespace floormat::Wall::detail;

void adl_serializer<std::shared_ptr<wall_atlas>>::to_json(json& j, const std::shared_ptr<const wall_atlas>& x)
{
    fm_assert(x != nullptr);
    write_info_header(j, x->info());
    write_all_directions(j, *x);
}

void adl_serializer<std::shared_ptr<wall_atlas>>::from_json(const json& j, std::shared_ptr<wall_atlas>& x)
{

}

} // namespace nlohmann
