#include "app.hpp"
#include "floormat/main.hpp"
#include "compat/format.hpp"
#include "imgui-raii.hpp"
#include <Magnum/Math/Color.h>

namespace floormat {

using namespace floormat::imgui;

void app::init_imgui(Vector2i size)
{
    if (!_imgui.context())
        _imgui = ImGuiIntegration::Context(Vector2{size}, size, size);
    else
        _imgui.relayout(Vector2{size}, size, size);
}

void app::render_menu()
{
    _imgui.drawFrame();
}

float app::draw_main_menu()
{
    float main_menu_height = 0;
    if (auto b = begin_main_menu())
    {
        ImGui::SetWindowFontScale(M->dpi_scale().min());
        if (auto b = begin_menu("File"))
        {
#if 0
            ImGui::MenuItem("Open", "Ctrl+O");
            ImGui::MenuItem("Recent");
            ImGui::Separator();
            ImGui::MenuItem("Save", "Ctrl+S");
            ImGui::MenuItem("Save as...", "Ctrl+Shift+S");
            ImGui::Separator();
            ImGui::MenuItem("Close");
            ImGui::Separator();
#endif
            bool do_new = false, do_quickload = false, do_quit = false;
            ImGui::MenuItem("New", nullptr, &do_new);
            ImGui::Separator();
            ImGui::MenuItem("Load quicksave", nullptr, &do_quickload);
            ImGui::Separator();
            ImGui::MenuItem("Quit", "Ctrl+Q", &do_quit);
            if (do_new)
                do_key(key_new_file, kmod_none);
            else if (do_quickload)
                do_key(key_quickload, kmod_none);
            else if (do_quit)
                do_key(key_quit, kmod_none);
        }
        if (auto b = begin_menu("Mode"))
        {
            bool can_rotate = false;
            if (auto* ed = _editor.current_tile_editor())
                can_rotate = ed->is_anything_selected();
            else if (auto* ed = _editor.current_scenery_editor())
                can_rotate = ed->is_anything_selected();
            auto mode = _editor.mode();
            using m = editor_mode;
            bool b_none = mode == m::none, b_floor = mode == m::floor, b_walls = mode == m::walls,
                 b_rotate = false, b_scenery = mode == m::scenery, b_collisions = _enable_render_bboxes;
            if (ImGui::MenuItem("Select",  "1", &b_none))
                do_key(key_mode_none);
            if (ImGui::MenuItem("Floor",   "2", &b_floor))
                do_key(key_mode_floor);
            if (ImGui::MenuItem("Walls",   "3", &b_walls))
                do_key(key_mode_walls);
            if (ImGui::MenuItem("Scenery", "4", &b_scenery))
                do_key(key_mode_scenery);
            if (ImGui::MenuItem("Show collisions", "Alt+C", &b_collisions))
                do_key(key_mode_collisions);
            ImGui::Separator();
            if (ImGui::MenuItem("Rotate", "R", &b_rotate, can_rotate))
                do_key(key_rotate_tile);
        }

        main_menu_height = ImGui::GetContentRegionMax().y;
    }
    return main_menu_height;
}

void app::draw_ui()
{
    const auto dpi = M->dpi_scale().min();
    [[maybe_unused]] const auto style_ = style_saver{};
    auto& style = ImGui::GetStyle();
    auto& ctx = *ImGui::GetCurrentContext();

    ImGui::StyleColorsDark(&style);
    style.ScaleAllSizes(dpi);

    ImGui::GetIO().IniFilename = nullptr;
    _imgui.newFrame();

    const float main_menu_height = draw_main_menu();
    [[maybe_unused]] auto font = font_saver{ctx.FontSize*dpi};
    if (_editor.current_tile_editor() || _editor.current_scenery_editor())
        draw_editor_pane(main_menu_height);
    draw_fps();
    draw_tile_under_cursor();
    if (_editor.mode() == editor_mode::none)
        draw_inspector();
    ImGui::EndFrame();
}

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

void app::draw_editor_pane(float main_menu_height)
{
    auto* ed = _editor.current_tile_editor();
    auto* sc = _editor.current_scenery_editor();
    fm_assert(!ed || !sc);

    const auto window_size = M->window_size();
    const auto dpi = M->dpi_scale();

    if (const bool active = M->is_text_input_active();
        ImGui::GetIO().WantTextInput != active)
        active ? M->start_text_input() : M->stop_text_input();

    [[maybe_unused]] const raii_wrapper vars[] = {
        push_style_var(ImGuiStyleVar_WindowPadding, {8*dpi[0], 8*dpi[1]}),
        push_style_var(ImGuiStyleVar_WindowBorderSize, 0),
        push_style_var(ImGuiStyleVar_FramePadding, {4*dpi[0], 4*dpi[1]}),
        push_style_color(ImGuiCol_WindowBg, {0, 0, 0, .5}),
        push_style_color(ImGuiCol_FrameBg, {0, 0, 0, 0}),
    };

    const auto& style = ImGui::GetStyle();

    if (main_menu_height > 0)
    {
        const auto b = push_id("editor");

        ImGui::SetNextWindowPos({0, main_menu_height+style.WindowPadding.y});
        ImGui::SetNextFrameWantCaptureKeyboard(false);
        ImGui::SetNextWindowSize({425 * dpi[0], window_size[1] - main_menu_height - style.WindowPadding.y});
        if (const auto flags = ImGuiWindowFlags_(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            auto b = begin_window({}, flags))
        {
            const auto b2 = push_id("editor-pane");
            if (auto b3 = begin_list_box("##atlases", {-FLT_MIN, -1}))
            {
                if (ed)
                    for (const auto& [k, v] : *ed)
                        draw_editor_tile_pane_atlas(*ed, k, v);
                else if (sc)
                    draw_editor_scenery_pane(*sc);
            }
        }
    }
}

void app::do_escape()
{
    if (auto* ed = _editor.current_scenery_editor())
        ed->clear_selection();
    if (auto* ed = _editor.current_tile_editor())
        ed->clear_selection();
    ImGui::FocusWindow(nullptr);
}

} // namespace floormat
