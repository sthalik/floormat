#include "atlas.hpp"
#include "serialize/anim.hpp"
#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include "compat/assert.hpp" // must go below opencv headers
using namespace floormat::Serialize;

void anim_atlas_row::add_entry(const anim_atlas_entry& x)
{
    auto& frame = *x.frame;
    const auto& mat = x.mat;
    frame.offset = {xpos, ypos};
    frame.size = {(unsigned)mat.cols, (unsigned)mat.rows};

    fm_assert(mat.rows > 0 && mat.cols > 0);
    data.push_back(x);
    xpos += (unsigned)mat.cols;
    max_height = std::max((unsigned)mat.rows, max_height);
}

void anim_atlas::advance_row()
{
    auto& row = rows.back();
    if (row.data.empty())
        return;
    fm_assert(row.xpos > 0);
    fm_assert(row.max_height > 0);
    ypos += row.max_height;
    maxx = std::max(row.xpos, maxx);
    rows.push_back({{}, 0, 0, ypos});
}

Magnum::Vector2ui anim_atlas::offset() const noexcept
{
    const auto& row = rows.back();
    return {row.xpos, row.ypos};
}

Magnum::Vector2ui anim_atlas::size() const noexcept
{
    const anim_atlas_row& row = rows.back();
    // prevent accidentally writing out of bounds by forgetting to call
    // anim_atlas::advance_row() one last time prior to anim_atlas::size()
    return {std::max(maxx, row.xpos), ypos + row.max_height};
}

bool anim_atlas::dump(StringView filename) const
{
    auto sz = size();
    cv::Mat4b mat((int)sz[1], (int)sz[0]);
    mat.setTo(0);

    for (const anim_atlas_row& row : rows)
        for (const anim_atlas_entry& x : row.data)
        {
            auto offset = x.frame->offset;
            auto size = x.frame->size;
            cv::Rect roi = {(int)offset[0], (int)offset[1], (int)size[0], (int)size[1]};
            x.mat.copyTo(mat(roi));
        }

    return cv::imwrite(filename.data(), mat);
}
