#include "camera-offset.hpp"
#include "tile-constants.hpp"
#include "depth.hpp"
#include "shaders/shader.hpp"

namespace floormat {

Vector2i with_shifted_camera_offset::get_projected_chunk_offset(chunk_coords_ c)
{
    // Chunk origins have even x+y, so the halving is exact.
    return tile_shader::projectʹ(Vector3i(Vector2i(c.x, c.y) * chunk_size<i32>, c.z * tile_size_z))/2;
}

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, chunk_coords_ c_) :
    _shader{shader},
    _camera{shader.camera_offset()}
{
    _shader.set_camera_offset(_camera + Vector2d(get_projected_chunk_offset(c_)));
}

with_shifted_camera_offset::~with_shifted_camera_offset() noexcept
{
    _shader.set_camera_offset(_camera);
}

} // namespace floormat
