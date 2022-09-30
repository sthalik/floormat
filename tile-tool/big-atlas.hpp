#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <opencv2/core/mat.hpp>

namespace std::filesystem { class path; }

struct big_atlas_frame {
    cv::Mat4b frame;
    Magnum::Vector2i position;
};

struct big_atlas_row {
    std::vector<big_atlas_frame> frames;
    int xpos = 0, ypos = 0;
};

struct big_atlas_builder {
    [[nodiscard]] std::vector<big_atlas_frame> add_atlas(const std::filesystem::path& filename);
    big_atlas_frame add_frame(const cv::Mat4b& frame);
    big_atlas_row& maybe_next_row();

private:
    static constexpr Magnum::Vector2i TILE_SIZE = { 100, 100 },
                                      MAX_TEXTURE_SIZE = { 512, 512 };

    std::vector<big_atlas_row> rows = {{}};
    int ypos = 0, maxx = 0;

    static_assert(!!TILE_SIZE[0] && !!TILE_SIZE[1] && !!MAX_TEXTURE_SIZE[0] && !!MAX_TEXTURE_SIZE[1]);
    static_assert(MAX_TEXTURE_SIZE[0] >= TILE_SIZE[0] && MAX_TEXTURE_SIZE[1] >= TILE_SIZE[1]);
};
