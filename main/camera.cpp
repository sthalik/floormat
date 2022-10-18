#include "app.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>

namespace floormat {

void app::do_camera(float dt_)
{
    constexpr int pixels_per_second = 768;
    const auto dt = (double)dt_;
    auto camera_offset = _shader.camera_offset();

    if (keys[key::camera_up])
        camera_offset += Vector2d{0,  1} * dt * pixels_per_second;
    else if (keys[key::camera_down])
        camera_offset += Vector2d{0, -1} * dt * pixels_per_second;
    if (keys[key::camera_left])
        camera_offset += Vector2d{1,  0} * dt * pixels_per_second;
    else if (keys[key::camera_right])
        camera_offset += Vector2d{-1, 0} * dt * pixels_per_second;

    {
        const auto max_camera_offset = Vector2d(windowSize() * 10);
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
#if 1
    _shader.set_camera_offset(tile_shader::project({TILE_MAX_DIM*-.5*dTILE_SIZE[0], TILE_MAX_DIM*-.5*dTILE_SIZE[1], 0}));
#else
    _shader.set_camera_offset({});
#endif
    recalc_cursor_tile();
}

void app::update_window_scale(Vector2i sz)
{
    _shader.set_scale(Vector2{sz});
}

void app::recalc_cursor_tile()
{
    if (_cursor_pixel)
        _cursor_tile = pixel_to_tile(Vector2d(*_cursor_pixel));
    else
        _cursor_tile = std::nullopt;
}

global_coords app::pixel_to_tile(Vector2d position) const
{
    const Vector2d px = position - Vector2d{windowSize()}*.5 - _shader.camera_offset()*.5;
    const Vector2d vec = tile_shader::unproject(px) / Vector2d{dTILE_SIZE[0]*.5, dTILE_SIZE[1]*.5} + Vector2d{.5, .5};
    const auto x = (std::int32_t)std::floor(vec[0]), y = (std::int32_t)std::floor(vec[1]);
    return { x, y };
}

std::array<std::int16_t, 4> app::get_draw_bounds() const noexcept
{
    const auto center = pixel_to_tile(Vector2d(windowSize()/2)).chunk();
    constexpr auto N = 1;

    return {
        std::int16_t(center.x - N), std::int16_t(center.x + N),
        std::int16_t(center.y - N), std::int16_t(center.y + N),
    };
}

} // namespace floormat
