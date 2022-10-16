#include "app.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>

namespace floormat {

void app::do_camera(float dt)
{
    constexpr float pixels_per_second = 256;
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
    camera_offset = project({TILE_MAX_DIM*TILE_SIZE[0]/2.f, TILE_MAX_DIM*TILE_SIZE[1]/2.f, 0});
    //camera_offset = {};
}

void app::update_window_scale(Vector2i sz)
{
    _shader.set_scale(Vector2{sz});
}

Vector2 app::pixel_to_tile(Vector2 position) const
{
    const auto px = position - Vector2{windowSize()}*.5f - camera_offset;
    return unproject(px) / Vector2{TILE_SIZE[0]*.5f, TILE_SIZE[1]*.5f} + Vector2{.5f, .5f};
}

void app::draw_cursor_tile()
{
    if (_cursor_pos)
    {
        const auto tile = pixel_to_tile(Vector2(*_cursor_pos));
        if (std::min(tile[0], tile[1]) >= 0 && std::max(tile[0], tile[1]) < (int)TILE_MAX_DIM)
        {
            const auto x = std::uint8_t(tile[0]), y = std::uint8_t(tile[1]);
            draw_wireframe_quad({x, y});
        }
    }
}

} // namespace floormat
