#include "app.hpp"
#include "tile-defs.hpp"
#include "camera-offset.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

namespace floormat {

void app::drawEvent()
{
    if (const float dt = timeline.previousFrameDuration(); dt > 0)
    {
        constexpr float RC = 0.5f;
        const float alpha = dt/(dt + RC);

        if (_frame_time > 0)
            _frame_time = _frame_time*(1-alpha) + alpha*dt;
        else
            _frame_time = dt;
    }
    else
    {
        swapBuffers();
        timeline.nextFrame();
    }

    {
        const float dt = std::clamp(timeline.previousFrameDuration(), 1e-3f, 1e-1f);
        update(dt);
    }

    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);
    _shader.set_tint({1, 1, 1, 1});
    {
        const with_shifted_camera_offset o{_shader, BASE_X, BASE_Y};
        draw_world();
        draw_cursor_tile();
    }

    display_menu();

    swapBuffers();
    redraw();
    timeline.nextFrame();
}

void app::draw_world()
{
#if 0
    _floor_mesh.draw(_shader, *_world[chunk_coords{0, 0}]);
    _wall_mesh.draw(_shader, *_world[chunk_coords{0, 0}]);
#else
    auto foo = get_draw_bounds();
    auto [minx, maxx, miny, maxy] = foo;

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            const chunk_coords c{x, y};
            if (!_world.contains(c))
                make_test_chunk(*_world[c]);
            const with_shifted_camera_offset o{_shader, c};
            _floor_mesh.draw(_shader, *_world[c]);
        }

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            const chunk_coords c{x, y};
            const with_shifted_camera_offset o{_shader, c};
            _wall_mesh.draw(_shader, *_world[c]);
        }
#endif
}

void app::draw_wireframe_quad(global_coords pos)
{
    constexpr float LINE_WIDTH = 1;
    const auto pt = pos.to_signed();

    if (const auto& [c, tile] = _world[pos]; tile.ground_image)
    {
        const Vector3 center{pt[0]*TILE_SIZE[0], pt[1]*TILE_SIZE[1], 0};
        _shader.set_tint({1, 0, 0, 1});
        _wireframe_quad.draw(_shader, {center, {TILE_SIZE[0], TILE_SIZE[1]}, LINE_WIDTH});
    }
}

void app::draw_wireframe_box(local_coords pt)
{
    constexpr float LINE_WIDTH = 1.5;

    constexpr auto X = TILE_SIZE[0], Y = TILE_SIZE[1], Z = TILE_SIZE[2];
    constexpr Vector3 size{X, Y, Z};
    const Vector3 center1{X*pt.x, Y*pt.y, 0};
    _shader.set_tint({0, 1, 0, 1});
    _wireframe_box.draw(_shader, {center1, size, LINE_WIDTH});
}

void app::draw_cursor_tile()
{
    if (_cursor_tile && !_cursor_in_imgui)
        draw_wireframe_quad(*_cursor_tile);
}

} // namespace floormat
