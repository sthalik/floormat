#include "app.hpp"
#include "tile-defs.hpp"
#include "camera-offset.hpp"
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Trade/AbstractImporter.h>

namespace floormat {

void app::drawEvent() {
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);
    GL::Renderer::setDepthFunction(GL::Renderer::DepthFunction::Never);

    {
        const float dt = std::min(1.f/10, timeline.previousFrameDuration());
        update(dt);
    }

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
    std::int16_t minx = BASE_X, maxx = BASE_X, miny = BASE_Y, maxy = BASE_Y;
    {
        const auto fn = [&](std::int32_t x, std::int32_t y) {
            const auto pos = pixel_to_tile({float(x + BASE_X), float(y + BASE_Y)}).chunk();
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
    fflush(stdout);
    return {minx, maxx, miny, maxy};
}

void app::draw_world()
{
    auto [minx, maxx, miny, maxy] = get_draw_bounds();

    _shader.set_tint({1, 1, 1, 1});

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            auto c = _world[chunk_coords{x, y}];
            _floor_mesh.draw(_shader, *c);
        }

    for (std::int16_t y = miny; y <= maxy; y++)
        for (std::int16_t x = minx; x <= maxx; x++)
        {
            auto c = _world[chunk_coords{x, y}];
            _wall_mesh.draw(_shader, *c);
        }
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
