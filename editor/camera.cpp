#include "app.hpp"
#include "src/global-coords.hpp"
#include "shaders/tile-shader.hpp"
#include "main/floormat-main.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>

namespace floormat {

void app::do_camera(float dt)
{
    if (_keys[key::camera_reset])
        reset_camera_offset();
    else
    {
        Vector2d dir{};

        if (_keys[key::camera_up])
            dir += Vector2d{0, -1};
        else if (_keys[key::camera_down])
            dir += Vector2d{0,  1};
        if (_keys[key::camera_left])
            dir += Vector2d{-1, 0};
        else if (_keys[key::camera_right])
            dir += Vector2d{1,  0};

        if (dir != Vector2d{})
        {
            auto& shader = M->shader();
            const auto sz = M->window_size();
            constexpr double screens_per_second = 0.75;

            const double pixels_per_second = sz.length() / screens_per_second;
            auto camera_offset = shader.camera_offset();
            const auto max_camera_offset = Vector2d(sz * 10);

            camera_offset -= dir.normalized() * (double)dt * pixels_per_second;
            camera_offset[0] = std::clamp(camera_offset[0], -max_camera_offset[0], max_camera_offset[0]);
            camera_offset[1] = std::clamp(camera_offset[1], -max_camera_offset[1], max_camera_offset[1]);

            Debug{} << "camera" << camera_offset;
            shader.set_camera_offset(camera_offset);
        }
        else
            return;
    }
    recalc_cursor_tile();
    if (cursor.tile)
        do_mouse_move(*cursor.tile);
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
