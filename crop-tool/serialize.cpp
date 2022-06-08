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
    { anim_direction::Invalid,  "x"     },
    { anim_direction::N,        "N"     },
    { anim_direction::NE,       "NE"    },
    { anim_direction::E,        "E"     },
    { anim_direction::SE,       "SE"    },
    { anim_direction::S,        "S"     },
    { anim_direction::SW,       "SW"    },
    { anim_direction::W,        "W"     },
    { anim_direction::NW,       "NW"    },
};

namespace nlohmann {

template<>
struct adl_serializer<Magnum::Vector2i> final {
    static void to_json(json& j, const Magnum::Vector2i& x)
    {
        j["x"] = x[0];
        j["y"] = x[1];
    }

    static void from_json(const json& j, Magnum::Vector2i& x)
    {
        j.at("x").get_to(x[0]);
        j.at("y").get_to(x[1]);
    }
};

template<>
struct adl_serializer<anim_direction> final {
    static void to_json(json& j, anim_direction x)
    {
        auto it = std::find_if(std::cbegin(anim_direction_map), std::cend(anim_direction_map),
                               [=](const auto& pair) { return x == pair.first; });
        if (it != std::cend(anim_direction_map))
            j = it->second;
        else
            j = anim_direction_map[0].second;
    }
    static void from_json(const json& j, anim_direction& x)
    {
        std::string str = j;
        auto it = std::find_if(std::cbegin(anim_direction_map), std::cend(anim_direction_map),
                               [&](const auto& pair) { return str == pair.second; });
        if (it != std::cend(anim_direction_map))
            x = it->first;
        else
            x = anim_direction::Invalid;
    }
};

} // namespace nlohmann

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_frame, ground);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_direction_group, group, frames);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim, name, nframes, actionframe, fps, directions);

std::optional<anim> anim::from_json(const std::filesystem::path& pathname)
{
    using namespace nlohmann;
    std::ifstream s;
    s.exceptions(s.exceptions() | std::ios::failbit);
    try {
        s.open(pathname, std::ios_base::in);
    } catch (const std::ios_base::failure& e) {
        Error{} << "failed to open" << pathname << ":" << e.what();
        return std::nullopt;
    }
    anim ret;
    try {
        json j;
        s >> j;
        using nlohmann::from_json;
        from_json(j, ret);
    } catch (const std::exception& e) {
        Error{} << "failed to parse" << pathname.string() << ":" << e.what();
    }
    return std::make_optional(std::move(ret));
}

const char* anim_direction_group::anim_direction_string(anim_direction group)
{
    auto it = std::find_if(std::cbegin(anim_direction_map), std::cend(anim_direction_map),
                           [=](const auto& pair) { return group == pair.first; });
    if (it != std::cend(anim_direction_map))
        return it->second;
    else
        return "(unknown)";
}
