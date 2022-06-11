#pragma once

#include "../defs.hpp"
#include <string>
#include <array>
#include <vector>
#include <optional>
#include <filesystem>
#include <Magnum/Trade/ImageData.h>

struct anim_frame final
{
    Magnum::Vector2i ground, offset, size;
};

enum class anim_direction : unsigned char
{
    N, NE, E, SE, S, SW, W, NW,
    COUNT = NW + 1,
};

struct anim_group final
{
    std::string name;
    std::vector<anim_frame> frames;
    Magnum::Vector2i ground;

    static const char* direction_to_string(anim_direction group);
    static anim_direction string_to_direction(const std::string& str);
};

struct anim final
{
    static std::optional<anim> from_json(const std::filesystem::path& pathname);
    bool to_json(const std::filesystem::path& pathname);
    static constexpr int default_fps = 24;

    std::string name;
    std::array<anim_group, (std::size_t)anim_direction::COUNT> groups;
    int nframes = 0, actionframe = -1, fps = default_fps;
};
