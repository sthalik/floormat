#include "camera-offset.hpp"
#include "tile-constants.hpp"
#include "shaders/shader.hpp"

namespace floormat {

Vector2d with_shifted_camera_offset::get_offset(chunk_coords_ c)
{
    return tile_shader::project(Vector3d(Vector2d(c.x, c.y) * TILE_MAX_DIM, c.z) * dTILE_SIZE);
}

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, chunk_coords_ c) :
    _shader{shader}, _camera{shader.camera_offset()}
{
    auto offset = _camera + get_offset(c);
    _shader.set_camera_offset(offset, float{1 << 24});
}

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, chunk_coords_ c_, chunk_coords first_, chunk_coords last_) :
    _shader{shader},
    _camera{shader.camera_offset()}
{
    (void)last_;
    fm_assert(shader.depth_offset() == 0.f);

    auto offset = _camera + get_offset(c_);
    auto pos  = chunk_coords(c_) - first_;
    constexpr auto depth_start = -1 + 1.111e-16f;

    int depth = (int)TILE_MAX_DIM*2 * pos.sum();

#if 0
    printf("c=(%2hd %2hd %2hhd) pos=(%2d %2d) len=(%d %d) --> %d\n", c_.x, c_.y, c_.z, pos.x(), pos.y(), len.x(), len.y(), depth);
#endif

    auto z_offset = (int{c_.z}-int{chunk_z_min}) * tile_shader::depth_value({}, tile_shader::z_depth_offset);
    auto d = depth * tile_shader::depth_tile_size + depth_start + z_offset;

    if (c_.z == chunk_z_max)
        d = 1;

    _shader.set_camera_offset(offset, d);
}

with_shifted_camera_offset::~with_shifted_camera_offset()
{
    _shader.set_camera_offset(_camera, 0);
}

} // namespace floormat
