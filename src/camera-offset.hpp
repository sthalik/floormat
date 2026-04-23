#pragma once
#include "global-coords.hpp"

namespace floormat {

struct tile_shader;

struct with_shifted_camera_offset final
{
    explicit with_shifted_camera_offset(tile_shader& shader, chunk_coords_ c);
    ~with_shifted_camera_offset() noexcept;

    static Vector2d get_projected_chunk_offset(chunk_coords_ c);
private:
    tile_shader& _shader; // NOLINT
    Vector2d _camera;
};

} // namespace floormat
