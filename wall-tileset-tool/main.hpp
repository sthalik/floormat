#pragma once
#include "src/wall-atlas.hpp"
#include <vector>
#include <opencv2/core/mat.hpp>

namespace floormat::wall_tool {

struct options
{
    String input_dir, input_file, output_dir;
};

struct frame
{
    cv::Mat4b mat;
    Vector2ui offset, size;
};

struct state
{
    options& opts;
    const wall_atlas_def& old_atlas;
    wall_atlas_def& new_atlas;
    std::vector<frame>& frames;
    cv::Mat4b& dest;
    int& error;
};

} // namespace floormat::wall_tool
