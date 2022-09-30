#undef NDEBUG

#include "atlas.hpp"
#include "../anim/serialize.hpp"

#include <cassert>
#include <filesystem>
#include <opencv2/imgcodecs.hpp>

void anim_atlas_row::add_entry(const anim_atlas_entry& x) noexcept
{
    auto& frame = *x.frame;
    const auto& mat = x.mat;
    frame.offset = {xpos, ypos};
    frame.size = {mat.cols, mat.rows};

    assert(mat.rows > 0 && mat.cols > 0);
    data.push_back(x);
    xpos += mat.cols;
    max_height = std::max(mat.rows, max_height);
}

void anim_atlas::advance_row() noexcept
{
    auto& row = rows.back();
    if (row.data.empty())
        return;
    assert(row.xpos); assert(row.max_height);
    ypos += row.max_height;
    maxx = std::max(row.xpos, maxx);
    rows.push_back({{}, 0, 0, ypos});
}

Magnum::Vector2i anim_atlas::offset() const noexcept
{
    const auto& row = rows.back();
    return {row.xpos, row.ypos};
}

Magnum::Vector2i anim_atlas::size() const noexcept
{
    const anim_atlas_row& row = rows.back();
    // prevent accidentally writing out of bounds by forgetting to call
    // anim_atlas::advance_row() one last time prior to anim_atlas::size()
    return {std::max(maxx, row.xpos), ypos + row.max_height};
}

bool anim_atlas::dump(const std::filesystem::path& filename) const noexcept
{
    auto sz = size();
    cv::Mat4b mat(sz[1], sz[0]);
    mat.setTo(0);

    for (const anim_atlas_row& row : rows)
        for (const anim_atlas_entry& x : row.data)
        {
            auto offset = x.frame->offset, size = x.frame->size;
            cv::Rect roi = {offset[0], offset[1], size[0], size[1]};
            x.mat.copyTo(mat(roi));
        }

    return cv::imwrite(filename.string(), mat);
}
