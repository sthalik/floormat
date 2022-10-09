#include "app.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>

namespace Magnum::Examples {

void app::do_camera(float dt)
{
    constexpr float pixels_per_second = 512;
    if (keys[key::camera_up])
        camera_offset += Vector2(0, 1) * dt * pixels_per_second;
    else if (keys[key::camera_down])
        camera_offset += Vector2(0, -1)  * dt * pixels_per_second;
    if (keys[key::camera_left])
        camera_offset += Vector2(1, 0) * dt * pixels_per_second;
    else if (keys[key::camera_right])
        camera_offset += Vector2(-1, 0)  * dt * pixels_per_second;

    _shader.set_camera_offset(camera_offset);

    if (keys[key::camera_reset])
        reset_camera_offset();
}

void app::reset_camera_offset()
{
    camera_offset = _shader.project({TILE_MAX_DIM*TILE_SIZE[0]/2.f, TILE_MAX_DIM*TILE_SIZE[1]/2.f, 0});
    //camera_offset = {};
}

void app::update_window_scale(Vector2i sz)
{
    _shader.set_scale(Vector2{sz});
}

void app::viewportEvent(Platform::Sdl2Application::ViewportEvent& event)
{
    update_window_scale(event.windowSize());
    GL::defaultFramebuffer.setViewport({{}, event.windowSize()});
}

} // namespace Magnum::Examples
