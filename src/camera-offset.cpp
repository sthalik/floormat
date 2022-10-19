#include "camera-offset.hpp"
#include "tile-defs.hpp"
#include "shaders/tile-shader.hpp"

namespace floormat {

static_assert(sizeof(short) == 2);

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, short x, short y) :
    _shader{shader},
    _offset{shader.camera_offset()}
{
    const auto offset = _offset +  tile_shader::project({double(x)*TILE_MAX_DIM*dTILE_SIZE[0],
                                                         double(y)*TILE_MAX_DIM*dTILE_SIZE[1],
                                                         0});
    _shader.set_camera_offset(offset);
}

with_shifted_camera_offset::~with_shifted_camera_offset()
{
    _shader.set_camera_offset(_offset);
}

} // namespace floormat
