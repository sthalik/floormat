#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

struct tile_shader;

struct with_shifted_camera_offset final
{
    explicit with_shifted_camera_offset(tile_shader& shader, short x, short y);
    ~with_shifted_camera_offset();
private:
    tile_shader& _shader; // NOLINT
    Vector2d _offset;
};

} // namespace floormat
