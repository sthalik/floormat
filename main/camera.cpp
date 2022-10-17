#include "app.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>

namespace floormat {

void app::do_camera(float dt)
{
    auto camera_offset = _shader.camera_offset();
        constexpr float pixels_per_second = 384;
    if (keys[key::camera_up])
        camera_offset += Vector2(0, 1) * dt * pixels_per_second;
    else if (keys[key::camera_down])
        camera_offset += Vector2(0, -1)  * dt * pixels_per_second;
    if (keys[key::camera_left])
        camera_offset += Vector2(1, 0) * dt * pixels_per_second;
    else if (keys[key::camera_right])
        camera_offset += Vector2(-1, 0)  * dt * pixels_per_second;

    {
        const auto max_camera_offset = Vector2(windowSize() * 10);
        camera_offset[0] = std::clamp(camera_offset[0], -max_camera_offset[0], max_camera_offset[0]);
        camera_offset[1] = std::clamp(camera_offset[1], -max_camera_offset[1], max_camera_offset[1]);
    }
    _shader.set_camera_offset(camera_offset);

    if (keys[key::camera_reset])
        reset_camera_offset();

    recalc_cursor_tile();
}

void app::reset_camera_offset()
{
    _shader.set_camera_offset(tile_shader::project({TILE_MAX_DIM/2.f*TILE_SIZE[0]/2.f, TILE_MAX_DIM/2.f*TILE_SIZE[1]/2.f, 0}));
    recalc_cursor_tile();
}

void app::update_window_scale(Vector2i sz)
{
    _shader.set_scale(Vector2{sz});
}

} // namespace floormat
