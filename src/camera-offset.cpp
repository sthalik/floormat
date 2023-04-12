#include "camera-offset.hpp"
#include "tile-defs.hpp"
#include "shaders/tile.hpp"

namespace floormat {

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, chunk_coords_ c_, chunk_coords first_, chunk_coords last_) :
    _shader{shader},
    _camera{shader.camera_offset()}
{
    fm_assert(shader.depth_offset() == 0.f);

    constexpr auto chunk_size = TILE_MAX_DIM20d*dTILE_SIZE;
    auto offset = _camera + tile_shader::project(Vector3d(c_.x, c_.y, 0) * chunk_size);
    auto pos  = Vector2d(chunk_coords(c_) - first_);
    auto len = Vector2d(last_ - first_) + Vector2d(1, 1);
    auto pos1 = pos.y() * len.x() + pos.x();
    auto z = c_.z - chunk_z_min;
    constexpr auto depth_start = -1 + 1.111e-16;

    double chunk_offset, tile_offset;

    if (c_.z < chunk_z_max)
    {
        chunk_offset = depth_start + tile_shader::depth_chunk_size * pos1;
        tile_offset = (double)tile_shader::depth_value({z, z});
    }
    else
    {
        chunk_offset = 1;
        tile_offset = 0;
    }

    double depth_offset_ = chunk_offset + tile_offset;
    auto depth_offset = (float)depth_offset_;

    _shader.set_camera_offset(offset, depth_offset);
}

with_shifted_camera_offset::~with_shifted_camera_offset()
{
    _shader.set_camera_offset(_camera, 0);
}

} // namespace floormat
