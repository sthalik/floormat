#include "camera-offset.hpp"
#include "tile-defs.hpp"
#include "shaders/tile.hpp"

namespace floormat {

static_assert(sizeof(short) == 2);

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, short x, short y) :
    _shader{shader},
    _offset{shader.camera_offset()}
{
    const auto offset = _offset +  tile_shader::project(Vector3d(x, y, 0) * TILE_MAX_DIM20d);
    _shader.set_camera_offset(offset);
}

with_shifted_camera_offset::~with_shifted_camera_offset()
{
    _shader.set_camera_offset(_offset);
}

} // namespace floormat
