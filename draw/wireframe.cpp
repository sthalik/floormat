#include "wireframe.hpp"
#include "shaders/shader.hpp"
#include <imgui.h>

namespace floormat::wireframe {

void draw_closed_polyline(tile_shader& shader, const vertex_array& corners, float line_width)
{
    // same world->screen mapping as app::point_screen_pos()
    const auto origin = Vector2(shader.camera_offset()) + shader.scale()*.5f;
    const auto tint = shader.tint();
    const auto color = ImGui::ColorConvertFloat4ToU32({tint[0], tint[1], tint[2], tint[3]});

    ImVec2 points[num_corners];
    for (auto i = 0u; i < num_corners; i++)
    {
        const auto pt = origin + tile_shader::project(corners[i]);
        points[i] = { pt[0], pt[1] };
    }

    // background list, so imgui windows stay on top as they did when this was a GL draw
    ImGui::GetBackgroundDrawList()->AddPolyline(points, (int)num_corners, color,
                                               line_width, ImDrawFlags_Closed);
}

} // namespace floormat::wireframe
