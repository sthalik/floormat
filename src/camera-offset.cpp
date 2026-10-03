#include "camera-offset.hpp"
#include "tile-constants.hpp"
#include "shaders/shader.hpp"
#include "depth.hpp"

namespace floormat {

Vector2i with_shifted_camera_offset::get_projected_chunk_offset2(chunk_coords_ c)
{
    return tile_shader::project2(Vector3i(Vector2i(c.x, c.y) * chunk_size<int32_t>, c.z * tile_size_z));
}

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, chunk_coords_ c_) :
    _shader{shader},
    _camera2{shader.camera2()}
{
    _shader.set_camera2(_camera2 + get_projected_chunk_offset2(c_));
}

with_shifted_camera_offset::~with_shifted_camera_offset() noexcept
{
    _shader.set_camera2(_camera2);
}

} // namespace floormat
