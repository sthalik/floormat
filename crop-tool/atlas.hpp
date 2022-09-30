#pragma once

#include <vector>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <opencv2/core/mat.hpp>

struct anim_frame;

namespace std::filesystem { class path; }

struct anim_atlas_entry
{
    anim_frame* frame;
    cv::Mat4b mat;
};

struct anim_atlas_row
{
    std::vector<anim_atlas_entry> data;
    int max_height = 0, xpos = 0, ypos = 0;

    void add_entry(const anim_atlas_entry& x) noexcept;
};

class anim_atlas
{
    std::vector<anim_atlas_row> rows = {{}};
    int ypos = 0, maxx = 0;

public:
    void add_entry(const anim_atlas_entry& x) noexcept { rows.back().add_entry(x); }
    void advance_row() noexcept;
    Magnum::Vector2i offset() const noexcept;
    Magnum::Vector2i size() const noexcept;
    [[nodiscard]] bool dump(const std::filesystem::path& filename) const noexcept;
};
