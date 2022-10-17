#include "camera-offset.hpp"
#include "shaders/tile-shader.hpp"

namespace floormat {

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, std::int32_t x, std::int32_t y) :
      with_shifted_camera_offset{shader, chunk_coords{std::int16_t(x), std::int16_t(y)}}
{
    ASSERT(std::abs(x) < (1 << 15) && std::abs(y) < (1 << 15));
}

with_shifted_camera_offset::with_shifted_camera_offset(tile_shader& shader, const chunk_coords c) :
    s{shader},
    orig_offset(shader.camera_offset())
{
    const auto offset = tile_shader::project({float(c.x)*TILE_MAX_DIM*TILE_SIZE[0],
                                              float(c.y)*TILE_MAX_DIM*TILE_SIZE[1],
                                              0});
    s.set_camera_offset(orig_offset + offset);
}

with_shifted_camera_offset::~with_shifted_camera_offset()
{
    s.set_camera_offset(orig_offset);
}

} // namespace floormat
