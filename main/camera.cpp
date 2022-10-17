#include "app.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>

namespace floormat {

void app::do_camera(float dt)
{
    auto camera_offset = _shader.camera_offset();
    constexpr float pixels_per_second = 768;
    if (keys[key::camera_up])
        camera_offset += Vector2{0,  1} * dt * pixels_per_second;
    else if (keys[key::camera_down])
        camera_offset += Vector2{0, -1} * dt * pixels_per_second;
    if (keys[key::camera_left])
        camera_offset += Vector2{1,  0} * dt * pixels_per_second;
    else if (keys[key::camera_right])
        camera_offset += Vector2{-1, 0} * dt * pixels_per_second;

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

    _shader.set_camera_offset(tile_shader::project({TILE_MAX_DIM*.25f*TILE_SIZE[0], TILE_MAX_DIM*.25f*TILE_SIZE[1], 0}));
    recalc_cursor_tile();
}

void app::update_window_scale(Vector2i sz)
{
    _shader.set_scale(Vector2{sz});
}

void app::recalc_cursor_tile()
{
    if (_cursor_pos)
    {
        constexpr Vector2 base_offset =
            tile_shader::project({(float)TILE_MAX_DIM*BASE_X*TILE_SIZE[0],
                                  (float)TILE_MAX_DIM*BASE_Y*TILE_SIZE[1], 0});
        _cursor_tile = pixel_to_tile(Vector2(*_cursor_pos) - base_offset);
    }
    else
        _cursor_tile = std::nullopt;
}

global_coords app::pixel_to_tile(Vector2 position) const
{
    const Vector2 px = position - Vector2{windowSize()}*.5f - _shader.camera_offset();
    const Vector2 vec = tile_shader::unproject(px) / Vector2{TILE_SIZE[0]*.5f, TILE_SIZE[1]*.5f} + Vector2{.5f, .5f};
    const auto x = (std::int32_t)std::floor(vec[0]), y = (std::int32_t)std::floor(vec[1]);
    return { x, y };
}

} // namespace floormat
