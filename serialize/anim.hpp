#pragma once

#include <tuple>
#include <array>
#include <vector>
#include <string>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <nlohmann/json_fwd.hpp>

namespace floormat::Serialize {

struct anim_frame final
{
    Vector2i ground, offset, size;
};

enum class anim_direction : unsigned char
{
    N, NE, E, SE, S, SW, W, NW,
    COUNT,
};

struct anim_group final
{
    std::string name;
    std::vector<anim_frame> frames;
    Vector2i ground;
};

struct anim final
{
    static constexpr int default_fps = 24;

    std::string object_name, anim_name;
    std::array<anim_group, (std::size_t)anim_direction::COUNT> groups;
    int nframes = 0;
    int width = 0, height = 0;
    int actionframe = -1, fps = default_fps;
};

} // namespace floormat::Serialize

namespace nlohmann {

template<>
struct adl_serializer<floormat::Serialize::anim_frame> {
    static void to_json(json& j, const floormat::Serialize::anim_frame& val);
    static void from_json(const json& j, floormat::Serialize::anim_frame& val);
};

template<>
struct adl_serializer<floormat::Serialize::anim_group> {
    static void to_json(json& j, const floormat::Serialize::anim_group& val);
    static void from_json(const json& j, floormat::Serialize::anim_group& val);
};

template<>
struct adl_serializer<floormat::Serialize::anim> {
    static void to_json(json& j, const floormat::Serialize::anim& val);
    static void from_json(const json& j, floormat::Serialize::anim& val);
};

} // namespace nlohmann
