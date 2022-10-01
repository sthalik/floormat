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
    big_atlas_frame& add_frame(const cv::Mat4b& frame);
    constexpr Magnum::Vector2i size() const { return {maxx, maxy}; }
    const std::vector<big_atlas_row>& rows() const { return _rows; }

private:
    std::tuple<big_atlas_row&, int, int> advance();
    std::vector<big_atlas_row> _rows = {{}};
    int ypos = 0, maxx = 0, maxy = 0;

    static constexpr Magnum::Vector2i TILE_SIZE = { 100, 100 },
                                      MAX_TEXTURE_SIZE = { 8192, 8192 };

    static_assert(!!TILE_SIZE[0] && !!TILE_SIZE[1] && !!MAX_TEXTURE_SIZE[0] && !!MAX_TEXTURE_SIZE[1]);
    static_assert(MAX_TEXTURE_SIZE[0] >= TILE_SIZE[0] && MAX_TEXTURE_SIZE[1] >= TILE_SIZE[1]);
};
