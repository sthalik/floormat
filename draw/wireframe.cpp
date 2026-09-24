#include "wireframe.hpp"
#include "shaders/shader.hpp"
#include <array>
#include <mg/Vector3.h>
#include <imgui.h>

namespace floormat::wireframe {

namespace {

constexpr uint32_t num_corners = 4;
using vertex_array = std::array<Vector3, num_corners>;

void draw_closed_polyline(tile_shader& shader, const vertex_array& corners, float line_width)
{
    // same world->screen mapping as app::point_to_pixel()
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

} // namespace

void draw_quad(tile_shader& shader, Vector3 center, Vector2 size, float line_width)
{
    const auto Sx = size[0]*.5f, Sy = size[1]*.5f;
    const auto Cx_0 = center[0] - Sx, Cx_1 = center[0] + Sx;
    const auto Cy_0 = center[1] - Sy, Cy_1 = center[1] + Sy;
    const auto Cz = center[2];
    draw_closed_polyline(shader, {{
        { Cx_0, Cy_0, Cz },
        { Cx_1, Cy_0, Cz },
        { Cx_1, Cy_1, Cz },
        { Cx_0, Cy_1, Cz },
    }}, line_width);
}

void draw_wall_n(tile_shader& shader, Vector3 center, Vector3 size, float line_width)
{
    const float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    const auto cx = center[0], cy = center[1], cz = center[2];
    draw_closed_polyline(shader, {{
        { -x + cx, -y + cy,     cz },
        {  x + cx, -y + cy,     cz },
        {  x + cx, -y + cy, z + cz },
        { -x + cx, -y + cy, z + cz },
    }}, line_width);
}

void draw_wall_w(tile_shader& shader, Vector3 center, Vector3 size, float line_width)
{
    const float x = size[0]*.5f, y = size[1]*.5f, z = size[2];
    const auto cx = center[0], cy = center[1], cz = center[2];
    draw_closed_polyline(shader, {{
        { -x + cx, -y + cy,     cz },
        { -x + cx,  y + cy,     cz },
        { -x + cx,  y + cy, z + cz },
        { -x + cx, -y + cy, z + cz },
    }}, line_width);
}

} // namespace floormat::wireframe
