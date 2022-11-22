#include "camera-offset.hpp"
#include "tile-defs.hpp"
#include "shaders/tile.hpp"

namespace floormat {

static_assert(sizeof(short) == 2);

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, short x, short y) :
    _shader{shader},
    _offset{shader.camera_offset()}
{
    constexpr auto chunk_size = TILE_MAX_DIM20d*dTILE_SIZE;
    const auto offset = _offset + tile_shader::project(Vector3d(x, y, 0) * chunk_size);
    _shader.set_camera_offset(offset);
}

with_shifted_camera_offset::~with_shifted_camera_offset()
{
    _shader.set_camera_offset(_offset);
}

} // namespace floormat
