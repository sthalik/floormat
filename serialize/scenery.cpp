#include "scenery.hpp"
#include "anim-atlas.hpp"
#include "compat/assert.hpp"
#include "loader/loader.hpp"
#include "serialize/corrade-string.hpp"
#include <array>
#include <Corrade/Containers/StringStlView.h>
#include <nlohmann/json.hpp>

namespace {

using namespace floormat;

constexpr struct {
    scenery_type value = scenery_type::none;
    StringView str;
} scenery_type_map[] = {
    { scenery_type::none,    "none"_s    },
    { scenery_type::generic, "generic"_s },
    { scenery_type::door,    "door"_s    },
    { scenery_type::object,  "object"_s  },
};

constexpr struct {
    rotation value = rotation::N;
    StringView str;
} rotation_map[] = {
    { rotation::N,  "n"_s  },
    { rotation::NE, "ne"_s },
    { rotation::E,  "e"_s  },
    { rotation::SE, "se"_s },
    { rotation::S,  "s"_s  },
    { rotation::SW, "sw"_s },
    { rotation::W,  "w"_s  },
    { rotation::NW, "nw"_s },
};

template<std::size_t N, typename T>
auto foo_from_string(StringView str, const T(&map)[N], const char* desc)
{
    for (const auto& [value, str2] : map)
        if (str2 == str)
            return value;
    fm_abort("wrong %s string '%s'", desc, str.data());
}

template<std::size_t N, typename T>
StringView foo_to_string(auto type, const T(&map)[N], const char* desc)
{
    for (const auto& [type2, str] : map)
        if (type2 == type)
            return str;
    fm_abort("wrong %s enum '%hhu'", desc, type);
}

} // namespace

namespace floormat {

} // namespace floormat

namespace nlohmann {

void adl_serializer<scenery_type>::to_json(json& j, const scenery_type val)
{
    j = foo_to_string(val, scenery_type_map, "scenery_type");
}

void adl_serializer<scenery_type>::from_json(const json& j, scenery_type& val)
{
    val = foo_from_string(j, scenery_type_map, "scenery_type");
}

void adl_serializer<rotation>::to_json(json& j, const rotation val)
{
    j = foo_to_string(val, rotation_map, "rotation");
}

void adl_serializer<rotation>::from_json(const json& j, rotation& val)
{
    val = foo_from_string(j, rotation_map, "rotation");
}

void adl_serializer<scenery_proto>::to_json(json& j, const scenery_proto& val)
{
    const scenery& f = val.frame;
    j["type"] = f.type;
    fm_assert(val.atlas);
    j["atlas-name"] = val.atlas->name();
    fm_assert(f.frame != scenery::NO_FRAME);
    j["frame"] = f.frame;
    j["rotation"] = f.r;
    j["passable"] = f.passable;
    j["blocks-view"] = f.blocks_view;
    j["active"] = f.active;
}

void adl_serializer<scenery_proto>::from_json(const json& j, scenery_proto& val)
{
    auto& f = val.frame;
    f.type = j["type"];
    StringView atlas_name = j["atlas-name"];
    val.atlas = loader.anim_atlas(atlas_name);
    f = {};
    if (j.contains("frame"))
        f.frame = j["frame"];
    if (j.contains("animated"))
        f.animated = j["animated"];
    fm_assert(f.animated == (f.frame == scenery::NO_FRAME));
    if (j.contains("rotation"))
        f.r = j["rotation"];
    if (j.contains("passable"))
        f.passable = j["passable"];
    if (j.contains("blocks-view"))
        f.blocks_view = j["blocks-view"];
    if (j.contains("active"))
        f.active = j["active"];
}

} // namespace nlohmann
