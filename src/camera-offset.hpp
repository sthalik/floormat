#pragma once
#include "global-coords.hpp"
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

struct tile_shader;

struct with_shifted_camera_offset final
{
    explicit with_shifted_camera_offset(tile_shader& shader, short x, short y);
    explicit with_shifted_camera_offset(tile_shader& shader, chunk_coords c) : with_shifted_camera_offset(shader, c.x, c.y) {}
    ~with_shifted_camera_offset();
private:
    tile_shader& _shader; // NOLINT
    Vector2d _offset;
};

} // namespace floormat
