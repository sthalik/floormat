#include "serialize/magnum-vector.hpp"
#include "serialize/magnum-vector2i.hpp"
#include "serialize/corrade-string.hpp"
#include "serialize/anim.hpp"
#include <tuple>

namespace floormat {

//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_frame, ground, offset, size)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_group, name, frames, ground, offset)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_def, object_name, anim_name, pixel_size, nframes, actionframe, fps, groups, scale)

static void to_json(nlohmann::json& j, const anim_frame& val)
{
    constexpr anim_frame def;

    if (val.ground != def.ground)
        j["ground"] = val.ground;
    if (val.offset != def.offset)
        j["offset"] = val.offset;
    if (val.size != def.size)
        j["size"] = val.size;
}

static void from_json(const nlohmann::json& j, anim_frame& val)
{
    val = {};
    if (j.contains("ground"))
        val.ground = j["ground"];
    if (j.contains("offset"))
        val.offset = j["offset"];
    if (j.contains("size"))
        val.size = j["size"];
}

static void to_json(nlohmann::json& j, const anim_group& val)
{
    const anim_group def{};

    j["name"] = val.name;
    if (val.mirror_from)
        j["mirror-from"] = val.mirror_from;
    else
        j["frames"] = val.frames;
    if (val.ground != def.ground)
        j["ground"] = val.ground;
    if (val.offset != def.offset)
        j["offset"] = val.offset;
    if (val.z_offset != def.z_offset)
        j["z-offset"] = val.z_offset;
    if (val.depth_offset != def.depth_offset)
        j["depth-offset"] = val.depth_offset;
}

static void from_json(const nlohmann::json& j, anim_group& val)
{
    val = {};
    val.name = j["name"];
    fm_soft_assert(!val.name.isEmpty());
    if (j.contains("mirror-from"))
    {
        fm_soft_assert(!j.contains("frames"));
        val.mirror_from = j["mirror-from"];
    }
    else
        val.frames = j["frames"];
    if (j.contains("ground"))
        val.ground = j["ground"];
    if (j.contains("offset"))
        val.offset = j["offset"];
    if (j.contains("z-offset"))
        val.z_offset = j["z-offset"];
    if (j.contains("depth-offset"))
        val.depth_offset = j["depth-offset"];
}

static void to_json(nlohmann::json& j, const anim_def& val)
{
    const anim_def def{};

    j["object_name"] = val.object_name;
    if (val.anim_name != def.anim_name)
        j["anim_name"] = val.anim_name;
    if (val.pixel_size != def.pixel_size)
        j["pixel_size"] = val.pixel_size;
    if (val.nframes != def.nframes)
        j["nframes"] = val.nframes;
    if (val.action_frame != def.action_frame)
        j["action-frame"] = val.action_frame;
    if (val.action_frame2 != def.action_frame2)
        j["action-frame-2"] = val.action_frame2;
    if (val.fps != def.fps)
        j["fps"] = val.fps;
    j["groups"] = val.groups;
    j["scale"] = val.scale;
}

static void from_json(const nlohmann::json& j, anim_def& val)
{
    val = {};
    val.object_name = j["object_name"];
    fm_soft_assert(!val.object_name.isEmpty());
    if (j.contains("anim_name")) // todo underscore to hyphen
        val.anim_name = j["anim_name"];
    if (j.contains("pixel_size"))
        val.pixel_size = j["pixel_size"];
    if (j.contains("nframes"))
        val.nframes = j["nframes"];
    if (j.contains("action-frame"))
        val.action_frame = j["action-frame"];
    if (j.contains("action-frame-2"))
        val.action_frame2 = j["action-frame-2"];
    if (j.contains("fps"))
        val.fps = j["fps"];
    val.groups = j["groups"];
    fm_soft_assert(!val.groups.empty());
    val.scale = j["scale"];
    fm_soft_assert(val.scale.type != anim_scale_type::invalid);
}

} // namespace floormat

using namespace floormat;

namespace nlohmann {

void adl_serializer<floormat::anim_scale>::to_json(json& j, const anim_scale val)
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

void adl_serializer<anim_scale>::from_json(const json& j, anim_scale& val)
{
    fm_soft_assert(j.is_array());
    fm_soft_assert(j.size() == 2);
    StringView type = j[0];
    if (type == "factor"_s)
    {
        auto factor = (float)j[1];
        fm_soft_assert(factor > 0 && factor <= 1);
        val = anim_scale::ratio{factor};
    }
    else if (bool is_width = type == "width"_s; is_width || type == "height"_s)
    {
        val = anim_scale::fixed{(unsigned)j[1], is_width};
        fm_soft_assert(val.f.width_or_height > 0);
    }
    else
        fm_throw("invalid anim_scale_type '{}'"_cf, type);
}

void adl_serializer<anim_frame>::to_json(json& j, const anim_frame& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim_frame>::from_json(const json& j, anim_frame& val) { using nlohmann::from_json; from_json(j, val); }
void adl_serializer<anim_group>::to_json(json& j, const anim_group& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim_group>::from_json(const json& j, anim_group& val) { using nlohmann::from_json; from_json(j, val); }
void adl_serializer<anim_def>::to_json(json& j, const anim_def& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim_def>::from_json(const json& j, anim_def& val) { using nlohmann::from_json; from_json(j, val); }

} // namespace nlohmann
