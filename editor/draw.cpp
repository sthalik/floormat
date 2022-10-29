#include "app.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "shaders/tile.hpp"
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/Math/Vector3.h>

namespace floormat {

void app::draw_wireframe_quad(global_coords pos)
{
    constexpr float LINE_WIDTH = 2;
    const auto pt = pos.to_signed();
    auto& shader = M->shader();

    {
        const Vector3 center{Vector3i(pt[0], pt[1], 0) * iTILE_SIZE};
        shader.set_tint({1, 0, 0, 1});
        _wireframe_quad.draw(shader, {center, TILE_SIZE2, LINE_WIDTH});
        //_wireframe_wall_n.draw(shader, {center, TILE_SIZE, LINE_WIDTH});
        //_wireframe_wall_w.draw(shader, {center, TILE_SIZE, LINE_WIDTH});
    }
}

void app::draw_wireframe_box(global_coords pos)
{
    constexpr float LINE_WIDTH = 1.5;
    auto& shader = M->shader();

    const auto pt = pos.to_signed();
    const auto center = Vector3((float)pt[0], (float)pt[1], 0) * TILE_SIZE;
    shader.set_tint({0, 1, 0, 1});
    _wireframe_box.draw(shader, {center, TILE_SIZE, LINE_WIDTH});
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
