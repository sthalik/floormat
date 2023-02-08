#include "serialize/magnum-vector.hpp"
#include "serialize/magnum-vector2i.hpp"
#include "serialize/corrade-string.hpp"
#include "serialize/anim.hpp"
#include <tuple>

namespace floormat {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_frame, ground, offset, size)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_group, name, frames, ground, offset)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_def, object_name, anim_name, pixel_size, nframes, actionframe, fps, groups, scale)

} // namespace floormat

using namespace floormat;

namespace nlohmann {

void adl_serializer<floormat::anim_scale>::to_json(json& j, const floormat::anim_scale val)
{
    switch (val.type)
    {
    default:
        fm_throw("invalid anim_scale_type '{}"_cf, (unsigned)val.type);
    case anim_scale_type::invalid:
        fm_throw("anim_scale is invalid"_cf);
    case anim_scale_type::fixed:
        j = std::tuple<StringView, unsigned>{val.f.is_width ? "width"_s : "height"_s, val.f.width_or_height};
        break;
    case anim_scale_type::ratio:
        j = std::tuple<StringView, float>{"factor"_s, val.r.f};
        break;
    }
}

void adl_serializer<floormat::anim_scale>::from_json(const json& j, floormat::anim_scale& val)
{
    fm_soft_assert(j.is_array());
    fm_soft_assert(j.size() == 2);
    String type = j[0];
    if (type == "factor"_s)
    {
        auto factor = (float)j[1];
        fm_soft_assert(factor > 0 && factor <= 1);
        val = { { .r = {factor} }, anim_scale_type::ratio };
    }
    else if (bool is_width = type == "width"_s; is_width || type == "height"_s)
    {
        val = { { .f = {is_width, (unsigned)j[1]} }, anim_scale_type::fixed };
        fm_soft_assert(val.f.width_or_height > 0);
    }
    else
        fm_throw("invalid anim_scale_type '{}'"_cf, type);
}

void adl_serializer<anim_frame>::to_json(json& j, const anim_frame& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim_frame>::from_json(const json& j, anim_frame& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<anim_group>::to_json(json& j, const anim_group& val)
{
    using nlohmann::to_json;
    if (!val.mirror_from.isEmpty())
    {
        j["name"] = val.name;
        j["mirror-from"] = val.mirror_from;
        j["offset"] = val.offset;
    }
    else
        to_json(j, val);
}

void adl_serializer<anim_group>::from_json(const json& j, anim_group& val)
{
    using nlohmann::from_json;
    if (j.contains("mirror-from"))
    {
        val.name = j["name"];
        val.mirror_from = j["mirror-from"];
        val.offset = j["offset"];
    }
    else
        from_json(j, val);
}

void adl_serializer<anim_def>::to_json(json& j, const anim_def& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim_def>::from_json(const json& j, anim_def& val) { using nlohmann::from_json; from_json(j, val); }

} // namespace nlohmann
