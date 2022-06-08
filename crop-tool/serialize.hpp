#pragma once

#include <string>
#include <array>
#include <vector>
#include <optional>
#include <filesystem>
#include <Magnum/Trade/ImageData.h>

struct anim_frame
{
    Magnum::Vector2i ground = {};
};

enum class anim_direction : unsigned char
{
    Invalid,
    N, NE, E, SE, S, SW, W, NW,
    MIN = N, MAX = NW,
};

struct anim_direction_group
{
    static const char* anim_direction_string(anim_direction group);
    anim_direction group;
    std::vector<anim_frame> frames;

    Magnum::Vector2i ground = {};
};

struct anim
{
    std::string name;
    std::array<anim_direction_group, (unsigned)anim_direction::MAX> directions;
    int nframes = 0, actionframe = 0, fps = default_fps;

    static std::optional<anim> from_json(const std::filesystem::path& pathname);

    static constexpr int default_fps = 24;
};
