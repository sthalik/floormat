#pragma once
#include "src/wall-atlas.hpp"

namespace cv {
template<typename T> class Mat_;
template<typename T, int cn> class Vec;
typedef Vec<unsigned char, 4> Vec4b;
typedef Mat_<Vec4b> Mat4b;
} // namespace cv

namespace floormat::wall_tool {

struct options
{
    String input_dir, input_file, output_dir;
};

struct state
{
    options& opts;
    cv::Mat4b& buffer;
    const wall_atlas_def& old_atlas;
    wall_atlas_def& new_atlas;
    int& error;
};

} // namespace floormat::wall_tool
