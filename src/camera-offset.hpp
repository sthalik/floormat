#pragma once
#include "global-coords.hpp"

namespace floormat {

struct tile_shader;

struct with_shifted_camera_offset final
{
    explicit with_shifted_camera_offset(tile_shader& shader, chunk_coords_ c);
    ~with_shifted_camera_offset() noexcept;

    static Vector2i get_projected_chunk_offset2(chunk_coords_ c);
private:
    tile_shader& _shader; // NOLINT
    Vector2i _camera2;
};

} // namespace floormat
