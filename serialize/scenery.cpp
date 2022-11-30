#include "scenery.hpp"
#include "anim-atlas.hpp"
#include "compat/assert.hpp"
#include "loader/loader.hpp"
#include "serialize/corrade-string.hpp"
#include <Corrade/Containers/StringStlView.h>
#include <nlohmann/json.hpp>

namespace {

using namespace floormat;
using namespace floormat::Serialize;

constexpr struct {
    scenery_type value = scenery_type::none;
    StringView str;
} scenery_type_map[] = {
    { scenery_type::none,    "none"_s    },
    { scenery_type::generic, "generic"_s },
    { scenery_type::door,    "door"_s    },
    //{ scenery_type::object,  "object"_s  },
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
    fm_abort("wrong %s enum '%zu'", desc, (std::size_t)type);
}

} // namespace

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
    j["frame"] = f.frame;
    j["rotation"] = f.r;
    j["passable"] = f.passable;
    j["blocks-view"] = f.blocks_view;
    j["active"] = f.active;
    j["interactive"] = f.interactive;
}

void adl_serializer<scenery_proto>::from_json(const json& j, scenery_proto& val)
{
    const auto get = [&](const StringView& name, auto& value)
    {
        auto s = std::string_view{name.data(), name.size()};
        if (j.contains(s))
            value = j[s];
    };

    StringView atlas_name = j["atlas-name"];
    fm_assert(!atlas_name.isEmpty());
    val.atlas = loader.anim_atlas(atlas_name, loader_::SCENERY_PATH);
    auto& f = val.frame;
    f = {};

    auto type = scenery_type::generic; get("type", type);
    auto frame       = f.frame;       get("frame", frame);
    auto r           = f.r;           get("rotation", r);
    bool passable    = f.passable;    get("passable", passable);
    bool blocks_view = f.blocks_view; get("blocks-view", blocks_view);
    bool active      = f.active;      get("active", active);

    switch (type)
    {
    default:
        fm_abort("unhandled scenery type '%u'", (unsigned)type);
    case scenery_type::generic:
        f = { scenery::generic, *val.atlas, r, frame, passable, blocks_view, active };
        break;
    case scenery_type::door:
        f = { scenery::door, *val.atlas, r, false };
    }
}

void adl_serializer<serialized_scenery>::to_json(json& j, const serialized_scenery& val)
{
    fm_assert(val.proto.atlas);
    j = val.proto;
    const auto name = !val.name.isEmpty() ? StringView{val.name} : val.proto.atlas->name();
    j["name"] = name;
    j["description"] = val.descr;
}

void adl_serializer<serialized_scenery>::from_json(const json& j, serialized_scenery& val)
{
    val = {};
    val.proto = j;
    val.name = j["name"];
    if (j.contains("description"))
        val.descr = j["description"];
}

} // namespace nlohmann
