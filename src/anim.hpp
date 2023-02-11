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
    String name, mirror_from;
    std::vector<anim_frame> frames;
    Vector2ui ground;
    Vector3b offset;
};

enum class anim_scale_type : std::uint8_t { invalid, ratio, fixed, };

struct anim_scale final
{
    struct ratio { float f; };
    struct fixed { bool is_width; unsigned width_or_height; };
    struct empty {};
    union {
        struct ratio r;
        struct fixed f;
        struct empty e = {};
    };
    anim_scale_type type = anim_scale_type::invalid;
    Vector2ui scale_to(Vector2ui image_size) const;
    Vector2 scale_to_(Vector2ui image_size) const;
};

struct anim_def final
{
    String object_name, anim_name;
    std::vector<anim_group> groups;
    Vector2ui pixel_size;
    anim_scale scale;
    std::size_t nframes = 0, fps = 0, actionframe = 0;
};

} // namespace floormat
