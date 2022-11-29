#include "app.hpp"
#include "src/global-coords.hpp"
#include "shaders/tile.hpp"
#include "floormat/main.hpp"
#include <algorithm>

namespace floormat {

void app::do_camera(float dt, const key_set& cmds, int mods)
{
    if (cmds[key_camera_reset])
    {
        reset_camera_offset();
        update_cursor_tile(cursor.pixel);
        do_mouse_move(mods);
        return;
    }

    Vector2d dir{};

    if (cmds[key_camera_up])
        dir += Vector2d{0, -1};
    else if (cmds[key_camera_down])
        dir += Vector2d{0,  1};
    if (cmds[key_camera_left])
        dir += Vector2d{-1, 0};
    else if (cmds[key_camera_right])
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

        shader.set_camera_offset(camera_offset, shader.depth_offset());

        update_cursor_tile(cursor.pixel);
        do_mouse_move(mods);
    }
}

void app::reset_camera_offset()
{
    constexpr Vector3d size = TILE_MAX_DIM20d*dTILE_SIZE*-.5;
    constexpr auto projected = tile_shader::project(size);
    M->shader().set_camera_offset(projected, 0);
    update_cursor_tile(cursor.pixel);
}

void app::update_cursor_tile(const Optional<Vector2i>& pixel)
{
    cursor.pixel = pixel;
    if (const auto [p, b] = pixel; b)
        cursor.tile = M->pixel_to_tile(Vector2d{p});
    else
        cursor.tile = NullOpt;
}

} // namespace floormat
