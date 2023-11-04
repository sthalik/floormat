#include "wall-atlas.hpp"
#include "magnum-vector2i.hpp"
#include "magnum-vector.hpp"
#include "compat/exception.hpp"
#include <utility>
#include <Corrade/Containers/StringStl.h>
#include <nlohmann/json.hpp>

// todo add test on dummy files that generates 100% coverage on the j.contains() blocks!

namespace floormat {

namespace {

constexpr StringView rotation_names[] = { "n"_s, "e"_s, "s"_s, "w"_s, };

size_t rotation_from_name(StringView s)
{
    for (auto i = 0uz; auto n : rotation_names)
    {
        if (n == s)
            return i;
        i++;
    }
    fm_throw("bad rotation name '{}'"_cf, fmt::string_view{s.data(), s.size()});
}

void read_frameset_metadata(const nlohmann::json& j, wall_frames& val)
{
    if (j.contains("pixel-size"))
        val.pixel_size = j["pixel-size"];
    if (j.contains("tint"))
    {
        auto& t = j["tint"];
        fm_soft_assert(t.contains("mult") || t.contains("add"));
        if (t.contains("mult"))
            val.tint_mult = Vector4(t["mult"]);
        if (t.contains("add"))
            val.tint_add = Vector3(t["add"]);
        fm_soft_assert(val.tint_mult >= Color4{0});
    }
    if (j.contains("from-rotation"))
        val.from_rotation = (uint8_t)rotation_from_name(std::string{j["from-rotation"]});
    if (j.contains("mirrored"))
        val.mirrored = j["mirrored"];
    if (j.contains("use-default-tint"))
        val.use_default_tint = j["use-default-tint"];
}

void write_frameset_metadata(nlohmann::json& j, const wall_atlas& a, const wall_frames& val)
{
    constexpr wall_frames default_value;
#if 0
    fm_soft_assert(val.index      != default_value.index);
    fm_soft_assert(val.count      != default_value.count);
    fm_soft_assert(val.pixel_size != default_value.pixel_size);
#endif

    fm_soft_assert(val.index < a.array().size());
    fm_soft_assert(val.count != (uint32_t)-1 && val.count > 0);
    j["pixel-size"] = val.pixel_size;
    if (val.tint_mult != default_value.tint_mult || val.tint_add != default_value.tint_add)
    {
        auto tint = std::pair<Vector4, Vector3>{{val.tint_mult}, {val.tint_add}};
        j["tint"] = tint;
    }
    if (val.from_rotation != default_value.from_rotation)
    {
        fm_soft_assert(val.from_rotation != (uint8_t)-1 && val.from_rotation < 4);
        j["from-rotation"] = val.from_rotation;
    }
    if (val.mirrored != default_value.mirrored)
        j["mirrored"] = val.mirrored;
    if (val.use_default_tint)
        if (val.tint_mult != default_value.tint_mult || val.tint_add != default_value.tint_add)
            j["use-default-tint"] = true;
}

} // namespace

} // namespace floormat

namespace nlohmann {

using namespace floormat;

void adl_serializer<std::shared_ptr<wall_atlas>>::to_json(json& j, const std::shared_ptr<const wall_atlas>& x)
{

}

void adl_serializer<std::shared_ptr<wall_atlas>>::from_json(const json& j, std::shared_ptr<wall_atlas>& x)
{

}

} // namespace nlohmann
