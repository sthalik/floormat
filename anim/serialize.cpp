#include "serialize.hpp"
#include "../json.hpp"

#include <algorithm>
#include <utility>
#include <fstream>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h>

using Corrade::Utility::Debug;
using Corrade::Utility::Error;

static constexpr
std::pair<anim_direction, const char*> anim_direction_map[] = {
    { anim_direction::N,        "n"     },
    { anim_direction::NE,       "ne"    },
    { anim_direction::E,        "e"     },
    { anim_direction::SE,       "se"    },
    { anim_direction::S,        "s"     },
    { anim_direction::SW,       "sw"    },
    { anim_direction::W,        "w"     },
    { anim_direction::NW,       "nw"    },
};

const char* anim_group::direction_to_string(anim_direction group)
{
    auto it = std::find_if(std::cbegin(anim_direction_map), std::cend(anim_direction_map),
                           [=](const auto& pair) { return group == pair.first; });
    if (it != std::cend(anim_direction_map))
        return it->second;
    else
        return "(unknown)";
}

anim_direction anim_group::string_to_direction(const std::string& str)
{
    auto it = std::find_if(std::cbegin(anim_direction_map), std::cend(anim_direction_map),
                           [&](const auto& pair) { return str == pair.second; });
    if (it != std::cend(anim_direction_map))
        return it->first;
    else
        return (anim_direction)0;
}

namespace nlohmann {

template<>
struct adl_serializer<anim_direction> final {
    static void to_json(json& j, anim_direction x);
    static void from_json(const json& j, anim_direction& x);
};

template<>
struct adl_serializer<Magnum::Vector2i> final {
    static void to_json(json& j, const Magnum::Vector2i& x);
    static void from_json(const json& j, Magnum::Vector2i& x);
};

void adl_serializer<Magnum::Vector2i>::to_json(json& j, const Magnum::Vector2i& x)
{
    j["x"] = x[0];
    j["y"] = x[1];
}

void adl_serializer<Magnum::Vector2i>::from_json(const json& j, Magnum::Vector2i& x)
{
    j.at("x").get_to(x[0]);
    j.at("y").get_to(x[1]);
}

#if 0
void adl_serializer<anim_direction>::to_json(json& j, anim_direction x)
{
    j = anim_group::direction_to_string(x);
}

void adl_serializer<anim_direction>::from_json(const json& j, anim_direction& x)
{
    x = anim_group::string_to_direction(j);
}
#endif

} // namespace nlohmann

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_frame, ground, offset, size);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_group, name, frames, ground);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim, name, nframes, actionframe, fps, groups);

std::optional<anim> anim::from_json(const std::filesystem::path& pathname)
{
    using namespace nlohmann;
    std::ifstream s;
    s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
    try {
        s.open(pathname, std::ios_base::in);
    } catch (const std::ios::failure& e) {
        Error{} << "failed to open" << pathname << ':' << e.what();
        return std::nullopt;
    }
    anim ret;
    try {
        json j;
        s >> j;
        using nlohmann::from_json;
        from_json(j, ret);
    } catch (const std::exception& e) {
        Error{} << "failed to parse" << pathname << ':' << e.what();
        return std::nullopt;
    }
    return std::make_optional(std::move(ret));
}

bool anim::to_json(const std::filesystem::path& pathname)
{
    nlohmann::json j = *this;

    std::ofstream s;
    s.exceptions(s.exceptions() | std::ios::failbit | std::ios::badbit);
    try {
        s.open(pathname, std::ios_base::out | std::ios_base::trunc);
    } catch (const std::ios::failure& e) {
        Error{} << "failed to open" << pathname << "for writing:" << e.what();
        return false;
    }
    s << j.dump(4);
    s.flush();

    return true;
}
