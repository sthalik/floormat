#include "app.hpp"
#include "floormat/main.hpp"
#include "src/tile-atlas.hpp"

#include <Magnum/GL/Renderer.h>
#include "imgui-raii.hpp"

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
            bool do_quit = false;
            ImGui::MenuItem("menu_quit", "Ctrl+Q", &do_quit);
            if (do_quit)
                do_key(key_quit, kmod_none);
        }
        if (auto b = begin_menu("Mode"))
        {
            const bool can_rotate = _editor.current_tile_editor() ? _editor.current_tile_editor()->can_rotate() : false;
            bool b_none = false, b_floor = false, b_walls = false, b_rotate = false;
            ImGui::MenuItem("Select", "1", &b_none);
            ImGui::MenuItem("Floor",  "2", &b_floor);
            ImGui::MenuItem("Walls",  "3", &b_walls);
            ImGui::Separator();
            ImGui::MenuItem("Rotate", "R", &b_rotate, can_rotate);
            if (b_none)
                do_key(key_mode_none);
            else if (b_floor)
                do_key(key_mode_floor);
            else if (b_walls)
                do_key(key_mode_walls);
            if (b_rotate)
                do_key(key_rotate_tile);
        }

        main_menu_height = ImGui::GetContentRegionMax().y;
    }
    return main_menu_height;
}

void app::draw_ui()
{
    ImGui::GetIO().IniFilename = nullptr;
    _imgui.newFrame();
    ImGui::StyleColorsDark(&ImGui::GetStyle());

    const float main_menu_height = draw_main_menu();
    if (auto* ed = _editor.current_tile_editor(); ed != nullptr)
        draw_editor_pane(*ed, main_menu_height);
    draw_fps();
    draw_cursor();
    draw_tile_under_cursor();
    ImGui::EndFrame();
}

static void draw_editor_pane_atlas(tile_editor& ed, StringView name, const std::shared_ptr<tile_atlas>& atlas)
{
    constexpr Color4 color_perm_selected{1, 1, 1, .7f},
                     color_selected{1, 0.843f, 0, .8f},
                     color_hover{0, .8f, 1, .7f};
    const float window_width = ImGui::GetWindowWidth() - 32;
    char buf[128];
    const auto& style = ImGui::GetStyle();
    const auto N = atlas->num_tiles();

    const auto click_event = [&] {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ed.select_tile_permutation(atlas);
        else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            ed.clear_selection();
    };
    const auto do_caption = [&] {
        click_event();
        if (ed.is_atlas_selected(atlas))
        {
            ImGui::SameLine();
            ImGui::Text(" (selected)");
        }
        std::snprintf(buf, sizeof(buf), "%zu", N);
        ImGui::SameLine(window_width - ImGui::CalcTextSize(buf).x - style.FramePadding.x - 4);
        ImGui::Text("%s", buf);
    };
    if (const auto flags = ImGuiTreeNodeFlags_(ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Framed);
        auto b = tree_node(name.data(), flags))
    {
        do_caption();
        [[maybe_unused]] const raii_wrapper vars[] = {
            push_style_var(ImGuiStyleVar_FramePadding, {2, 2}),
            push_style_color(ImGuiCol_ButtonHovered, color_hover),
        };
        const bool perm_selected = ed.is_permutation_selected(atlas);
        constexpr std::size_t per_row = 8;
        for (std::size_t i = 0; i < N; i++)
        {
            const bool selected = ed.is_tile_selected(atlas, i);

            if (i > 0 && i % per_row == 0)
                ImGui::NewLine();

            [[maybe_unused]] const raii_wrapper vars[] = {
                selected ? push_style_color(ImGuiCol_Button, color_selected) : raii_wrapper{},
                selected ? push_style_color(ImGuiCol_ButtonHovered, color_selected) : raii_wrapper{},
                perm_selected ? push_style_color(ImGuiCol_Button, color_perm_selected) : raii_wrapper{},
                perm_selected ? push_style_color(ImGuiCol_ButtonHovered, color_perm_selected) : raii_wrapper{},
            };

            std::snprintf(buf, sizeof(buf), "##item_%zu", i);
            const auto uv = atlas->texcoords_for_id(i);
            constexpr ImVec2 size_2 = { TILE_SIZE[0]*.5f, TILE_SIZE[1]*.5f };
            ImGui::ImageButton(buf, (void*)&atlas->texture(), size_2,
                               { uv[3][0], uv[3][1] }, { uv[0][0], uv[0][1] });
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                ed.select_tile(atlas, i);
            else
                click_event();
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }
    else
        do_caption();
}

void app::draw_editor_pane(tile_editor& ed, float main_menu_height)
{
    const auto window_size = M->window_size();

    if (const bool active = M->is_text_input_active();
        ImGui::GetIO().WantTextInput != active)
        active ? M->start_text_input() : M->stop_text_input();

    [[maybe_unused]] const raii_wrapper vars[] = {
        push_style_var(ImGuiStyleVar_WindowPadding, {8, 8}),
        push_style_var(ImGuiStyleVar_WindowBorderSize, 0),
        push_style_var(ImGuiStyleVar_FramePadding, {4, 4}),
        push_style_color(ImGuiCol_WindowBg, {0, 0, 0, .5}),
        push_style_color(ImGuiCol_FrameBg, {0, 0, 0, 0}),
    };

    const auto& style = ImGui::GetStyle();

    if (main_menu_height > 0)
    {
        ImGui::SetNextWindowPos({0, main_menu_height+style.WindowPadding.y});
        ImGui::SetNextFrameWantCaptureKeyboard(false);
        ImGui::SetNextWindowSize({420, window_size[1] - main_menu_height - style.WindowPadding.y});
        if (const auto flags = ImGuiWindowFlags_(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            auto b = begin_window({}, flags))
        {
            if (auto b = begin_list_box("##atlases", {-FLT_MIN, -1}))
            {
                for (const auto& [k, v] : ed)
                {
                    draw_editor_pane_atlas(ed, k, v);
                }
            }
        }
    }
}

void app::draw_fps()
{
    const auto frame_time = M->smoothed_dt();
    char buf[16];
    const double hz = frame_time > 1e-6f ? (int)std::round(10./(double)frame_time + .05) * .1 : 9999;
    snprintf(buf, sizeof(buf), "%.1f FPS", hz);
    const ImVec2 size = ImGui::CalcTextSize(buf);
    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    draw.AddText({M->window_size()[0] - size.x - 4, 3}, ImGui::ColorConvertFloat4ToU32({0, 1, 0, 1}), buf);
}

void app::draw_tile_under_cursor()
{
    if (!cursor.tile)
        return;

    char buf[64];
    const auto coord = *cursor.tile;
    const auto chunk = coord.chunk();
    const auto local = coord.local();
    snprintf(buf, sizeof(buf), "%hd:%hd - %hhu:%hhu", chunk.x, chunk.y, local.x, local.y);
    const auto size = ImGui::CalcTextSize(buf);
    const auto window_size = M->window_size();

    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    draw.AddText({window_size[0]*.5f - size.x/2, 3}, (unsigned)-1, buf);
}

} // namespace floormat
