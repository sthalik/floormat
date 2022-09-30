#undef NDEBUG

#include "big-atlas.hpp"
#include <cassert>
#include <filesystem>
#include <Corrade/Utility/DebugStl.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

std::vector<big_atlas_frame> big_atlas_builder::add_atlas(const std::filesystem::path& filename)
{
    using Corrade::Utility::Error;
    std::vector<big_atlas_frame> ret;
    cv::Mat mat = cv::imread(filename.string(), cv::IMREAD_UNCHANGED);
    if (mat.empty() || (mat.type() != CV_8UC4 && mat.type() != CV_8UC3))
    {
        Error{} << "failed to load" << filename << "as RGBA32 image";
        return {};
    }
    if (mat.type() == CV_8UC3) {
        cv::Mat mat2;
        cv::cvtColor(mat, mat2, cv::COLOR_RGB2RGBA);
        mat = mat2.clone();
    }

    Error{} << "file" << filename;

    assert(mat.cols % TILE_SIZE[0] == 0 && mat.rows % TILE_SIZE[1] == 0);

    for (int y = 0; y + TILE_SIZE[1] <= mat.rows; y += TILE_SIZE[1])
        for (int x = 0; x + TILE_SIZE[0] <= mat.cols; x += TILE_SIZE[0])
        {
            Error{} << "convert" << x << y;
            cv::Rect roi { x, y, TILE_SIZE[0], TILE_SIZE[1] };
            auto frame = add_frame(mat(roi));
            ret.push_back(frame);
        }

    return ret;
}

big_atlas_frame big_atlas_builder::add_frame(const cv::Mat4b& frame)
{
    auto& row = maybe_next_row();
    big_atlas_frame ret { frame, { row.xpos, row.ypos } };
    row.frames.push_back(ret);
    row.xpos += TILE_SIZE[0];
    maxx = std::max(maxx, row.xpos);
    return ret;
}

big_atlas_row& big_atlas_builder::maybe_next_row()
{
    auto& row = rows.back();

    if (row.xpos + TILE_SIZE[0] > MAX_TEXTURE_SIZE[0])
    {
        ypos += TILE_SIZE[1];
        rows.emplace_back();
        auto& row = rows.back();
        row.ypos = ypos;

        return row;
    }
    else
        return row;
}
