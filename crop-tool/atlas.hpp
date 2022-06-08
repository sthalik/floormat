#pragma once
#include <array>
#include <vector>
#include <Magnum/Trade/ImageData.h>

struct anim_frame
{
    Magnum::Trade::ImageData2D image;
    Magnum::Vector2us ground_offset = {};
};

enum class anim_direction : unsigned char
{
    Invalid,
    N, NE, E, SE, S, SW, W, NW,
    MIN = N, MAX = NW,
};

struct anim_direction_group
{
    std::vector<anim_frame> frames;
    anim_direction dir = anim_direction::Invalid;
};

struct anim
{
    std::array<anim_direction_group, (unsigned)anim_direction::MAX> directions;
    int num_frames = 0, action_frame = 0, fps = default_fps;

    static constexpr int default_fps = 24;
};
