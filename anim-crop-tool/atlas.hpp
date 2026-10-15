#pragma once
#include <vector>
#include <cr/StringView.h>
#include <mg/Vector2.h>
#include <opencv2/core/mat.hpp>

namespace floormat { struct anim_frame; }

namespace floormat::Serialize {

struct anim_atlas_entry
{
    cv::Mat4b mat;
    Magnum::Vector2ui offset, size;
};

struct anim_atlas_row
{
    std::vector<anim_atlas_entry> data;
    unsigned max_height = 0, xpos = 0, ypos = 0;

    void add_entry(anim_frame& frame, const cv::Mat4b& mat);
};

class anim_atlas_
{
    std::vector<anim_atlas_row> rows = {{}};
    unsigned ypos = 0, maxx = 0;

public:
    void add_entry(anim_frame& frame, const cv::Mat4b& mat);
    void advance_row();
    Magnum::Vector2ui offset() const noexcept;
    Magnum::Vector2ui size() const noexcept;
    [[nodiscard]] bool dump(StringView filename) const;
};

} // namespace floormat::Serialize
