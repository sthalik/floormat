#pragma once

#include "defs.hpp"

#include <string>
#include <array>
#include <vector>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

namespace std::filesystem { class path; }

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
};

struct anim final
{
    static std::tuple<anim, bool> from_json(const std::filesystem::path& pathname);
    [[nodiscard]] bool to_json(const std::filesystem::path& pathname);
    static constexpr int default_fps = 24;

    std::string name;
    std::array<anim_group, (std::size_t)anim_direction::COUNT> groups;
    int nframes = 0, actionframe = -1, fps = default_fps;
};
