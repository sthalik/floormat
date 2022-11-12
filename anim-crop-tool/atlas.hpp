#pragma once
#include <vector>
#include <Corrade/Containers/StringView.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <opencv2/core/mat.hpp>

namespace floormat { struct anim_frame; }

namespace floormat::Serialize {

struct anim_atlas_entry
{
    anim_frame* frame;
    cv::Mat4b mat;
};

struct anim_atlas_row
{
    std::vector<anim_atlas_entry> data;
    unsigned max_height = 0, xpos = 0, ypos = 0;

    void add_entry(const anim_atlas_entry& x);
};

class anim_atlas
{
    std::vector<anim_atlas_row> rows = {{}};
    unsigned ypos = 0, maxx = 0;

public:
    void add_entry(const anim_atlas_entry& x) { rows.back().add_entry(x); }
    void advance_row();
    Magnum::Vector2ui offset() const noexcept;
    Magnum::Vector2ui size() const noexcept;
    [[nodiscard]] bool dump(StringView filename) const;
};

} // namespace floormat::Serialize
