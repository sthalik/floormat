#include "camera-offset.hpp"
#include "tile-defs.hpp"
#include "shaders/tile-shader.hpp"
#include <Magnum/Math/Vector2.h>

namespace floormat {

static_assert(sizeof(short) == 2);

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, short x, short y) :
    _shader{shader},
    _offset{shader.camera_offset()}
{
    const auto offset = tile_shader::project({float(x)*TILE_MAX_DIM*TILE_SIZE[0],
                                              float(y)*TILE_MAX_DIM*TILE_SIZE[1],
                                              0});
    _shader.set_camera_offset(_offset + offset);
}

with_shifted_camera_offset::~with_shifted_camera_offset()
{
    _shader.set_camera_offset(_offset);
}

} // namespace floormat
