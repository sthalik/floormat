#include "app.hpp"
#include "main/floormat-main.hpp"
#include "shaders/tile-shader.hpp"

namespace floormat {

void app::draw_wireframe_quad(global_coords pos)
{
    constexpr float LINE_WIDTH = 2;
    const auto pt = pos.to_signed();
    auto& shader = M->shader();

    //if (const auto& [c, tile] = _world[pos]; tile.ground_image)
    {
        const Vector3 center{pt[0]*TILE_SIZE[0], pt[1]*TILE_SIZE[1], 0};
        shader.set_tint({1, 0, 0, 1});
        _wireframe_quad.draw(shader, {center, {TILE_SIZE[0], TILE_SIZE[1]}, LINE_WIDTH});
    }
}

void app::draw_wireframe_box(global_coords pos)
{
    constexpr float LINE_WIDTH = 1.5;
    auto& shader = M->shader();

    constexpr auto X = TILE_SIZE[0], Y = TILE_SIZE[1], Z = TILE_SIZE[2];
    constexpr Vector3 size{X, Y, Z};
    const auto pt = pos.to_signed();
    const Vector3 center{pt[0]*TILE_SIZE[0], pt[1]*TILE_SIZE[1], 0};
    shader.set_tint({0, 1, 0, 1});
    _wireframe_box.draw(shader, {center, size, LINE_WIDTH});
}

void app::draw_cursor_tile()
{
    if (cursor.tile && !cursor.in_imgui)
        draw_wireframe_quad(*cursor.tile);
}

void app::draw_msaa()
{
    draw_cursor_tile();
}

void app::draw()
{
    render_menu();
}

} // namespace floormat

