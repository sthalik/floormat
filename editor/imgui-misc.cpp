#include "app.hpp"
#include "floormat/main.hpp"
#include "imgui-raii.hpp"
#include "compat/format.hpp"

namespace floormat {
using namespace floormat::imgui;


void app::draw_fps()
{
    const auto dpi = M->dpi_scale();
    const auto frame_time = M->smoothed_dt();
    char buf[16];
    const double hz = frame_time > 1e-6f ? (int)std::round(10./(double)frame_time + .05) * .1 : 9999;
    snformat(buf, "{:.1f} FPS"_cf, hz);
    const ImVec2 size = ImGui::CalcTextSize(buf);
    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    draw.AddText(nullptr, ImGui::GetCurrentContext()->FontSize,
                 {M->window_size()[0] - size.x - 3.5f*dpi[0], 3*dpi[1]}, ImGui::ColorConvertFloat4ToU32({0, 1, 0, 1}), buf);
#if 0
    static auto timer = fm_begin( Timeline t; t.start(); return t; );
    if (timer.currentFrameDuration() >= 2)
    {
        timer.start();
        std::printf("FPS %f\n", (double)(frame_time > 1e-6f ? 1/frame_time : 0));
        std::fflush(stdout);
    }
#endif
}

void app::draw_tile_under_cursor()
{
    if (!cursor.tile)
        return;

    const auto dpi = M->dpi_scale();
    char buf[64];
    const auto coord = *cursor.tile;
    const auto chunk = coord.chunk();
    const auto local = coord.local();
    snformat(buf, "{}:{} - {}:{}"_cf, chunk.x, chunk.y, local.x, local.y);
    const auto size = ImGui::CalcTextSize(buf);
    const auto window_size = M->window_size();

    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    draw.AddText(nullptr, ImGui::GetCurrentContext()->FontSize,
                 {window_size[0]*.5f - size.x/2, 3*dpi[1]}, (unsigned)-1, buf);
}

} // namespace floormat