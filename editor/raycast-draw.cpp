#include "raycast-draw.hpp"
#include "app.hpp"
#include "imgui-raii.hpp"
#include "src/raycast-diag.hpp"
#include "shaders/shader.hpp"
#include <mg/Constants.h>
#include <mg/Functions.h>

namespace floormat {

namespace {

void draw_boxes(app& a, ImDrawList& draw, ArrayView<const detail_rc::bbox> boxes,
                ImU32 color, float thickness)
{
    for (auto [center, size] : boxes)
    {
        const auto hx = (int)(size.x()/2), hy = (int)(size.y()/2);
        const auto p00 = a.point_to_pixel(point::normalize_coords(center, {-hx, -hy})),
                   p10 = a.point_to_pixel(point::normalize_coords(center, { hx, -hy})),
                   p01 = a.point_to_pixel(point::normalize_coords(center, {-hx,  hy})),
                   p11 = a.point_to_pixel(point::normalize_coords(center, { hx,  hy}));
        draw.AddLine({p00.x(), p00.y()}, {p01.x(), p01.y()}, color, thickness);
        draw.AddLine({p00.x(), p00.y()}, {p10.x(), p10.y()}, color, thickness);
        draw.AddLine({p01.x(), p01.y()}, {p11.x(), p11.y()}, color, thickness);
        draw.AddLine({p10.x(), p10.y()}, {p11.x(), p11.y()}, color, thickness);
    }
}

} // namespace

void draw_raycast_line(app& a, const rc::raycast_result_s& r, float dot_size)
{
    const bool hit = !r.success;
    const auto line_color = ImGui::ColorConvertFloat4ToU32(hit ? ImVec4{1, 0, .75f, 1}
                                                               : ImVec4{0, .8f, .8f, .75f}),
               ring_color = ImGui::ColorConvertFloat4ToU32(ImVec4{0, 0, 1, 1}),
               dot_color  = ImGui::ColorConvertFloat4ToU32(hit ? ImVec4{1, 0, 0, 1}
                                                               : ImVec4{0, 1, 0, 1});
    ImDrawList& draw = *ImGui::GetForegroundDrawList();

    // Where the ray stopped -- the collider it met, or the target it reached. Every ray gets
    // one: a line ending in mid-air among four thousand stools says nothing about how far it got.
    const auto p0 = a.point_to_pixel(r.from),
               p1 = a.point_to_pixel(hit ? r.collision : r.to);
    draw.AddLine({p0.x(), p0.y()}, {p1.x(), p1.y()}, line_color, 2);
    draw.AddCircleFilled({p1.x(), p1.y()}, 10*dot_size, ring_color);
    draw.AddCircleFilled({p1.x(), p1.y()}, 7*dot_size, dot_color);
}

void draw_raycast_projectile(app& a, const rc::raycast_result_s& r, const rc::raycast_diag_s& diag)
{
    if (r.success || diag.radius <= 0)
        return;

    // AddEllipse would hard-code project()'s axes.
    // Sampling through project() follows any change to it.
    constexpr uint32_t n = 48;
    constexpr float step = 2.f * Math::Constants<float>::pi() / (float)n;
    const auto center = a.point_to_pixel(r.from) + tile_shader::project(Vector3(diag.dir * diag.tmin, 0));
    ImVec2 pts[n];
    for (auto i = 0u; i < n; i++)
    {
        const auto theta = Rad{(float)i * step};
        const auto p = center + tile_shader::project(Vector3(Vector2{Math::cos(theta), Math::sin(theta)} * diag.radius, 0));
        pts[i] = {p.x(), p.y()};
    }
    const auto color = ImGui::ColorConvertFloat4ToU32(ImVec4{1, .5f, 0, 1});
    ImGui::GetForegroundDrawList()->AddPolyline(pts, (int)n, color, 2.f, ImDrawFlags_Closed);
}

void draw_raycast_diag(app& a, const rc::raycast_diag_s& diag)
{
    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    draw_boxes(a, draw, diag.path, ImGui::ColorConvertFloat4ToU32(ImVec4{1, 0, 0, 1}), 1);
    draw_boxes(a, draw, diag.queries, ImGui::ColorConvertFloat4ToU32(ImVec4{1, 1, 0, 1}), 2);
}

} // namespace floormat
