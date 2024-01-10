#pragma once
#include "src/wall-atlas.hpp"
#include <vector>
#include <opencv2/core/mat.hpp>

namespace floormat::wall_tool {

struct options
{
    String input_dir, input_file, output_dir;
    bool use_alpha = false;
};

struct frame
{
    cv::Mat4b mat;
    Vector2ui offset{}, size;
};

struct group
{
    std::vector<frame> frames;
    Wall::Group_ G;
};

struct state
{
    options& opts;
    const wall_atlas_def& old_atlas;
    wall_atlas_def& new_atlas;
    std::vector<group>& groups;
    size_t& n_frames;
    cv::Mat4b& dest;
    int& error;
};

} // namespace floormat::wall_tool
