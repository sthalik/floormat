#include "app.hpp"
#include "tile-defs.hpp"
#include "camera-offset.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

namespace floormat {

void app::drawEvent() {

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
        swapBuffers(), timeline.nextFrame();

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

std::array<std::int16_t, 4> app::get_draw_bounds() const noexcept
{
    using limits = std::numeric_limits<std::int16_t>;
    constexpr auto MIN = limits::min(), MAX = limits::max();
    std::int16_t minx = MAX, maxx = MIN, miny = MAX, maxy = MIN;
    {
        auto fn = [&](std::int32_t x, std::int32_t y) {
            const auto pos = pixel_to_tile(Vector2(x, y)).chunk();
            minx = std::min(minx, pos.x);
            maxx = std::max(maxx, pos.x);
            miny = std::min(miny, pos.y);
            maxy = std::max(maxy, pos.y);
        };
        const auto sz = windowSize();
        const auto x = sz[0]-1, y = sz[1]-1;
        fn(0, 0);
        fn(x, 0);
        fn(0, y);
        fn(x, y);
    }
    return {
        std::int16_t(minx), std::int16_t(maxx),
        std::int16_t(miny), std::int16_t(maxy),
    };
}

void app::draw_world()
{
    _floor_mesh.draw(_shader, *_world[chunk_coords{0, 0}]);
    _wall_mesh.draw(_shader, *_world[chunk_coords{0, 0}]);
#if 0
    auto [minx, maxx, miny, maxy] = get_draw_bounds();

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
            _floor_mesh.draw(_shader, *_world[chunk_coords{x, y}]);

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
            _wall_mesh.draw(_shader, *_world[chunk_coords{x, y}]);
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

    constexpr auto X = TILE_SIZE[0], Y = TILE_SIZE[1];
    constexpr Vector3 size{TILE_SIZE[0], TILE_SIZE[1], TILE_SIZE[2]*1.5f};
    const Vector3 center1{X*pt.x, Y*pt.y, 0};
    _shader.set_tint({0, 1, 0, 1});
    _wireframe_box.draw(_shader, {center1, size, LINE_WIDTH});
}

void app::draw_cursor_tile()
{
    if (_cursor_tile)
        draw_wireframe_quad(*_cursor_tile);
}

} // namespace floormat
