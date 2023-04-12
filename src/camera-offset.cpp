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
    auto z = (int)(c_.z - chunk_z_min);
    auto pos  = chunk_coords(c_) - first_;
    auto len = (last_ - first_) + Vector2i(1, 1);
    constexpr auto depth_start = -1 + 1.111e-16f;

    int depth = TILE_MAX_DIM * pos.x() +
                (int)TILE_COUNT * len.x() * pos.y() +
                z * (TILE_MAX_DIM+1);

#if 1
    if (c_.z == 0)
        printf("c=(%2hd %2hd %2hhd) pos=(%2d %2d) len=(%d %d) --> %d\n", c_.x, c_.y, c_.z, pos.x(), pos.y(), len.x(), len.y(), depth);
#endif

    float d = depth * tile_shader::depth_tile_size + depth_start;

    if (c_.z == chunk_z_max)
        d = 1;

    _shader.set_camera_offset(offset, d);
}

with_shifted_camera_offset::~with_shifted_camera_offset()
{
    _shader.set_camera_offset(_camera, 0);
}

} // namespace floormat
