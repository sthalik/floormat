#include "camera-offset.hpp"
#include "tile-constants.hpp"
#include "shaders/shader.hpp"
#include "depth.hpp"

namespace floormat {

Vector2d with_shifted_camera_offset::get_projected_chunk_offset(chunk_coords_ c)
{
    return tile_shader::project(Vector3d(Vector2d(c.x, c.y) * TILE_MAX_DIM, c.z) * dTILE_SIZE);
}

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, chunk_coords_ c_) :
    _shader{shader},
    _camera{shader.camera_offset()}
{
    auto offset = _camera + get_projected_chunk_offset(c_);
    _shader.set_camera_offset(offset);
}

with_shifted_camera_offset::~with_shifted_camera_offset() noexcept
{
    _shader.set_camera_offset(_camera);
}

} // namespace floormat
