#include "app.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include "shaders/tile.hpp"
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/Math/Vector3.h>

namespace floormat {

void app::draw_cursor_tile()
{
    constexpr float LINE_WIDTH = 2;

    if (cursor.tile && !cursor.in_imgui)
    {
        const auto pos = *cursor.tile;

        const auto draw = [&](auto& mesh, const auto& size) {
            const auto pt = pos.to_signed();
            const Vector3 center{Vector3i(pt[0], pt[1], 0) * iTILE_SIZE};
            auto& shader = M->shader();
            shader.set_tint({1, 0, 0, 1});
            mesh.draw(shader, {center, size, LINE_WIDTH});
        };

        if (const auto* ed = _editor.current(); ed && ed->mode() == editor_mode::walls)
        {
            switch (ed->rotation())
            {
            case editor_wall_rotation::N: draw(_wireframe_wall_n, TILE_SIZE); break;
            case editor_wall_rotation::W: draw(_wireframe_wall_w, TILE_SIZE); break;
            }
        }
        else
            draw(_wireframe_quad, TILE_SIZE2);
    }
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
