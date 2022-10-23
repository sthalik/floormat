#include "app.hpp"
#include "src/global-coords.hpp"
#include "shaders/tile-shader.hpp"
#include "main/floormat-main.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>

namespace floormat {

void app::do_camera(flota dt)
{
    if (keys[key::camera_reset])
        reset_camera_offset();
    else
    {
        Vector2d dir{};

        if (keys[key::camera_up])
            dir += Vector2d{0, -1};
        else if (keys[key::camera_down])
            dir += Vector2d{0,  1};
        if (keys[key::camera_left])
            dir += Vector2d{-1, 0};
        else if (keys[key::camera_right])
            dir += Vector2d{1,  0};

        if (dir != Vector2d{})
        {
            constexpr double screens_per_second = 1;
            const auto pixels_per_second = windowSize().length() / screens_per_second;
            auto camera_offset = _shader.camera_offset();
            const auto max_camera_offset = Vector2d(windowSize() * 10);

            camera_offset -= dir.normalized() * dt * pixels_per_second;
            camera_offset[0] = std::clamp(camera_offset[0], -max_camera_offset[0], max_camera_offset[0]);
            camera_offset[1] = std::clamp(camera_offset[1], -max_camera_offset[1], max_camera_offset[1]);

            _shader.set_camera_offset(camera_offset);
        }
        else
            return;
    }
    recalc_cursor_tile();
    if (cursor.tile)
        do_mouse_move(*_cursor_tile);
}

void app::reset_camera_offset()
{
    M->shader().set_camera_offset(tile_shader::project({TILE_MAX_DIM*-.5*dTILE_SIZE[0], TILE_MAX_DIM*-.5*dTILE_SIZE[1], 0}));
    recalc_cursor_tile();
}

void app::recalc_cursor_tile()
{
    if (cursor.pixel && !cursor.in_imgui)
        cursor.tile = M->pixel_to_tile(Vector2d(*cursor.pixel));
    else
        cursor.tile = std::nullopt;
}

} // namespace floormat
