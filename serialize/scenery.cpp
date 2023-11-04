#include "scenery.hpp"
#include "compat/exception.hpp"
#include "src/anim-atlas.hpp"
#include "compat/assert.hpp"
#include "loader/loader.hpp"
#include "serialize/corrade-string.hpp"
#include "loader/scenery.hpp"
#include "serialize/pass-mode.hpp"
#include "serialize/magnum-vector2i.hpp"
#include <Corrade/Containers/StringStlView.h>
#include <nlohmann/json.hpp>

namespace {

using namespace floormat;
using namespace std::string_literals;

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

template<size_t N, typename T>
auto foo_from_string(StringView str, const T(&map)[N], const char* desc)
{
    for (const auto& [value, str2] : map)
        if (str2 == str)
            return value;
    fm_throw("wrong {} string '{}'"_cf, desc, str);
}

template<size_t N, typename T>
StringView foo_to_string(auto type, const T(&map)[N], const char* desc)
{
    for (const auto& [type2, str] : map)
        if (type2 == type)
            return str;
    fm_throw("wrong {} enum '{}'"_cf, desc, (size_t)type);
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

void adl_serializer<scenery_proto>::to_json(json& j, const scenery_proto& f)
{
    fm_assert(f.atlas);
    const scenery_proto default_scenery;
    if (f.type != default_scenery.type)
        j["type"s] = f.type;
    j["atlas-name"s] = f.atlas->name();
    if (f.frame != default_scenery.frame)
        j["frame"s] = f.frame;
    if (f.r != default_scenery.r)
        j["rotation"s] = f.r;
    if (f.pass != default_scenery.pass)
        j["pass-mode"s] = f.pass;
    if (f.active != default_scenery.active)
        j["active"s] = f.active;
    if (f.interactive != default_scenery.interactive)
        j["interactive"s] = f.interactive;
    if (f.offset != default_scenery.offset)
        j["offset"s] = Vector2i(f.offset);
    if (f.bbox_offset != default_scenery.bbox_offset)
        j["bbox-offset"s] = Vector2i(f.bbox_offset);
    if (f.bbox_size != default_scenery.bbox_size)
        j["bbox-size"s] = Vector2ui(f.bbox_size);
}

void adl_serializer<scenery_proto>::from_json(const json& j, scenery_proto& f)
{
    const auto get = [&](const StringView& name, auto& value)
    {
        auto s = std::string_view{name.data(), name.size()};
        if (j.contains(s))
            value = j[s];
    };

    StringView atlas_name = j["atlas-name"s];
    fm_soft_assert(!atlas_name.isEmpty());
    f = {};
    f.atlas = loader.anim_atlas(atlas_name, loader_::SCENERY_PATH);

    auto type = scenery_type::generic;              get("type", type);
    auto frame       = f.frame;                     get("frame", frame);
    auto r           = f.atlas->first_rotation();   get("rotation", r);
    pass_mode pass   = f.pass;                      get("pass-mode", pass);
    bool active      = f.active;                    get("active", active);
    bool interactive = f.interactive;               get("interactive", interactive);
    auto offset      = Vector2i(f.offset);          get("offset", offset);
    auto bbox_offset = Vector2i(f.bbox_offset);     get("bbox-offset", bbox_offset);
    auto bbox_size   = Vector2ui(f.bbox_size);      get("bbox-size", bbox_size);
    fm_soft_assert(offset == Vector2i(Vector2b(offset)));
    fm_soft_assert(bbox_offset == Vector2i(Vector2b(bbox_offset)));
    fm_soft_assert(bbox_size == Vector2ui(Vector2ub(bbox_size)));

    switch (type)
    {
    default:
        fm_throw("unhandled scenery type '{}'"_cf, (unsigned)type);
    case scenery_type::generic:
        f.type = object_type::scenery;
        f.sc_type = scenery_type::generic;
        f.r = r;
        f.frame = frame;
        f.pass = pass;
        f.active = active;
        f.interactive = interactive;
        f.offset = Vector2b(offset);
        f.bbox_offset = Vector2b(bbox_offset);
        f.bbox_size = Vector2ub(bbox_size);
        break;
    case scenery_type::door:
        fm_assert(f.atlas->info().fps > 0 && f.atlas->info().nframes > 0);
        f.type = object_type::scenery;
        f.sc_type = scenery_type::door;
        f.r = r;
        f.frame = uint16_t(f.atlas->group(r).frames.size()-1);
        f.pass = pass_mode::blocked;
        f.interactive = true;
        f.closing = false;
        f.offset = Vector2b(offset);
        f.bbox_offset = Vector2b(bbox_offset);
        f.bbox_size = Vector2ub(bbox_size);
        break;
    }
}

void adl_serializer<serialized_scenery>::to_json(json& j, const serialized_scenery& val)
{
    fm_soft_assert(val.proto.atlas);
    j = val.proto;
    const auto name = !val.name.isEmpty() ? StringView{val.name} : val.proto.atlas->name();
    j["name"s] = name;
    j["description"s] = val.descr;
}

void adl_serializer<serialized_scenery>::from_json(const json& j, serialized_scenery& val)
{
    val = {};
    val.proto = j;
    val.name = j["name"s];
    if (j.contains("description"s))
        val.descr = j["description"s];
}

} // namespace nlohmann
