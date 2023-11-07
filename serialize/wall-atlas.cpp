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

    if (jgroup.contains("pixel-size"s))
        val.pixel_size = jgroup["pixel-size"s];
    if (jgroup.contains("tint"s))
        std::tie(val.tint_mult, val.tint_add) = std::pair<Vector4, Vector3>{ jgroup["tint"s] };
    if (jgroup.contains("from-rotation"s))
        val.from_rotation = (uint8_t)direction_index_from_name(std::string{ jgroup["from-rotation"s] });
    if (jgroup.contains("mirrored"s))
        val.mirrored = !!jgroup["mirrored"s];
    if (jgroup.contains("use-default-tint"s))
        val.use_default_tint = !!jgroup["use-default-tint"s];

    fm_soft_assert(val.tint_mult >= Color4{0});

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

void write_group_metadata(json& jgroup, const Group& val)
{
    constexpr Group default_value;

    fm_soft_assert(jgroup.is_object());
    fm_soft_assert(jgroup.empty());

    jgroup["pixel-size"s] = val.pixel_size;
    if (val.tint_mult != default_value.tint_mult || val.tint_add != default_value.tint_add)
    {
        auto tint = std::pair<Vector4, Vector3>{{val.tint_mult}, {val.tint_add}};
        jgroup["tint"s] = tint;
    }
    if (val.from_rotation != default_value.from_rotation)
    {
        fm_soft_assert(val.from_rotation != none && val.from_rotation < 4);
        jgroup["from-rotation"s] = val.from_rotation;
    }
    if (val.mirrored != default_value.mirrored)
        jgroup["mirrored"s] = val.mirrored;
    if (val.use_default_tint)
        if (val.tint_mult != default_value.tint_mult || val.tint_add != default_value.tint_add)
            jgroup["use-default-tint"s] = true;
}

Info read_info_header(const json& jroot)
{
    Info val { std::string(jroot["name"s]), jroot["depth"] };
    return val;
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
