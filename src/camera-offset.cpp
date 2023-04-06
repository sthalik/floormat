#include "camera-offset.hpp"
#include "tile-defs.hpp"
#include "shaders/tile.hpp"

namespace floormat {

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, chunk_coords c, chunk_coords first, chunk_coords last) :
    _shader{shader},
    _camera{shader.camera_offset()}
{
    fm_assert(shader.depth_offset() == 0.f);

    constexpr auto chunk_size = TILE_MAX_DIM20d*dTILE_SIZE;
    const auto offset = _camera + tile_shader::project(Vector3d(c) * chunk_size);
    first.x -= 8; first.y -= 8; last.x += 8; last.y += 8; // Z levels
    const auto len_x = (float)(last.x - first.x), cx = (float)(c.x - first.x), cy = (float)(c.y - first.y);
    const float depth_offset = shader.depth_tile_size*(cy*TILE_MAX_DIM*len_x*TILE_MAX_DIM + cx*TILE_MAX_DIM);

    _shader.set_camera_offset(offset, depth_offset);
}

with_shifted_camera_offset::~with_shifted_camera_offset()
{
    _shader.set_camera_offset(_camera, 0);
}

} // namespace floormat
