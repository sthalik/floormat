#include "wall-atlas.hpp"
#include "magnum-vector2i.hpp"
#include "magnum-vector.hpp"
#include "compat/exception.hpp"
#include <Corrade/Containers/StringStl.h>
#include <nlohmann/json.hpp>

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

void read_frameset_metadata(nlohmann::json& j, wall_frames& ret)
{
    if (j.contains("pixel-size"))
        ret.pixel_size = j["pixel-size"];
    if (j.contains("tint"))
    {
        auto& t = j["tint"];
        fm_soft_assert(t.contains("mult") || t.contains("add"));
        if (t.contains("mult"))
            ret.tint_mult = Vector4(t["mult"]);
        if (t.contains("add"))
            ret.tint_add = Vector3(t["add"]);
    }
    if (j.contains("from-rotation"))
        ret.from_rotation = (uint8_t)rotation_from_name(std::string{j["from-rotation"]});
    if (j.contains("mirrored"))
        ret.mirrored = j["mirrored"];
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
