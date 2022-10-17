#pragma once
#include "src/global-coords.hpp"

namespace floormat {

struct tile_shader;

struct with_shifted_camera_offset final
{
    explicit with_shifted_camera_offset(tile_shader& shader, std::int32_t, std::int32_t);
    explicit with_shifted_camera_offset(tile_shader& shader, chunk_coords c);
    ~with_shifted_camera_offset();
private:
    tile_shader& s; // NOLINT
    Vector2 orig_offset;
};

} // namespace floormat
