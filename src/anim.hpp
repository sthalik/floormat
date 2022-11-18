#pragma once

#include <vector>
#include <Corrade/Containers/String.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

namespace floormat {

struct anim_frame final
{
    Vector2i ground;
    Vector2ui offset, size;
};

enum class anim_direction : unsigned char
{
    N, NE, E, SE, S, SW, W, NW,
    COUNT,
};

struct anim_group final
{
    String name;
    std::vector<anim_frame> frames;
    Vector2ui ground;
    Vector3b offset;
};

struct anim_def final
{
    static constexpr int default_fps = 24;

    String object_name, anim_name;
    std::vector<anim_group> groups;
    Vector2ui pixel_size;
    std::size_t nframes = 0, width = 0, height = 0, fps = default_fps, actionframe = 0;
};

} // namespace floormat
