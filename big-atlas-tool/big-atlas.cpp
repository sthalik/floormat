#undef NDEBUG

#include "big-atlas.hpp"
#include <cassert>
#include <filesystem>
#include <Corrade/Utility/DebugStl.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

using Corrade::Utility::Error;

std::vector<big_atlas_frame> big_atlas_builder::add_atlas(const std::filesystem::path& filename)
{
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
        mat = std::move(mat2);
    }

    Error{} << "file" << filename;

    assert(mat.cols % TILE_SIZE[0] == 0 && mat.rows % TILE_SIZE[1] == 0);

    cv::Mat1b cn;

    for (int y = 0; y + TILE_SIZE[1] <= mat.rows; y += TILE_SIZE[1])
        for (int x = 0; x + TILE_SIZE[0] <= mat.cols; x += TILE_SIZE[0])
        {
            cv::Rect roi { x, y, TILE_SIZE[0], TILE_SIZE[1] };
            const auto m = mat(roi);
            cv::extractChannel(m, cn, 3);
            if (cv::countNonZero(cn) > 0)
            {
                auto frame = add_frame(m);
                ret.push_back(frame);
            }
        }

    return ret;
}

big_atlas_frame& big_atlas_builder::add_frame(const cv::Mat4b& frame)
{
    auto [row, xpos, ypos] = advance();
    row.frames.push_back({ frame, { xpos, ypos } });
    return row.frames.back();
}

std::tuple<big_atlas_row&, int, int> big_atlas_builder::advance()
{
    auto& row = _rows.back();
    const int xpos_ = row.xpos;
    row.xpos += TILE_SIZE[0];

    if (row.xpos + TILE_SIZE[0] > MAX_TEXTURE_SIZE[0])
    {
        ypos += TILE_SIZE[1];
        assert(ypos < MAX_TEXTURE_SIZE[1]);
        _rows.emplace_back();
        auto& row = _rows.back();
        row.ypos = ypos;
        row.xpos = 0;
        return { row, 0, row.ypos };
    }
    else {
        maxx = std::max(maxx, row.xpos);
        maxy = row.ypos + TILE_SIZE[1];
        return { row, xpos_, row.ypos };
    }
}
