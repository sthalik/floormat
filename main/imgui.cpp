#include "app.hpp"
#include <Magnum/GL/Renderer.h>
#ifndef __CLION_IDE__
#include "imgui-raii.hpp"
#include <Magnum/ImGuiIntegration/Integration.h>
#endif

namespace floormat {

using namespace floormat::imgui;

void app::display_menu()
{
    GL::Renderer::setBlendEquation(GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add);
    GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha, GL::Renderer::BlendFunction::OneMinusSourceAlpha);
    GL::Renderer::enable(GL::Renderer::Feature::Blending);

    GL::Renderer::enable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

    _imgui.drawFrame();
}

float app::draw_main_menu()
{
    float main_menu_height = 0;
    if (auto b = begin_main_menu())
    {
        if (auto b = begin_menu("File"))
        {
            ImGui::MenuItem("Open", "Ctrl+O");
            ImGui::MenuItem("Recent");
            ImGui::Separator();
            ImGui::MenuItem("Save", "Ctrl+S");
            ImGui::MenuItem("Save as...", "Ctrl+Shift+S");
            ImGui::Separator();
            ImGui::MenuItem("Close");
        }
        if (auto b = begin_menu("Mode"))
        {
            ImGui::MenuItem("Select", "F1", _editor.mode() == editor_mode::select);
            ImGui::MenuItem("Floor", "F2", _editor.mode() == editor_mode::floor);
            ImGui::MenuItem("Walls", "F3", _editor.mode() == editor_mode::walls);
        }

        main_menu_height = ImGui::GetContentRegionMax().y;
    }
    return main_menu_height;
}

void app::draw_ui()
{
    _imgui.newFrame();
    ImGui::StyleColorsDark(&ImGui::GetStyle());

    const float main_menu_height = draw_main_menu();
    draw_editor_pane(_editor.floor(), main_menu_height);
    draw_fps();
    draw_cursor_coord();
}

void app::draw_editor_pane(tile_type& type, float main_menu_height)
{
    if (ImGui::GetIO().WantTextInput && !isTextInputActive())
        startTextInput();
    else if (!ImGui::GetIO().WantTextInput && isTextInputActive())
        stopTextInput();

    const raii_wrapper vars[] = {
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
        ImGui::SetNextWindowSize({450, windowSize()[1] - main_menu_height - style.WindowPadding.y});
        if (const auto flags = ImGuiWindowFlags_(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            auto b = begin_window({}, flags))
        {
            const float window_width = ImGui::GetWindowWidth() - 32;

            char buf[64];
            //ImGui::SetNextWindowBgAlpha(.2f);

            if (auto b = begin_list_box("##tiles", {-FLT_MIN, -1}))
            {
                for (const auto& [k, v] : type)
                {
                    ///const auto& k_ = k;
                    const auto& v_ = v;
                    const auto click_event = [&] {
                        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                        _editor.floor().select_tile_permutation(v_);
                    };
                    const auto add_tile_count = [&] {
                        snprintf(buf, sizeof(buf), "%zu", (std::size_t)v_->num_tiles().product());
                        ImGui::SameLine(window_width - ImGui::CalcTextSize(buf).x - style.FramePadding.x - 4);
                        ImGui::Text("%s", buf);
                    };
                    const std::size_t N = v->num_tiles().product();
                    if (const auto flags = ImGuiTreeNodeFlags_(ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Framed);
                        auto b = tree_node(k.data(), flags))
                    {
                        click_event();
                        add_tile_count();
                        const raii_wrapper vars[] = {
                            push_style_var(ImGuiStyleVar_FramePadding, {1, 1}),
                            push_style_var(ImGuiStyleVar_FrameBorderSize, 3),
                            push_style_color(ImGuiCol_Button, {1, 1, 1, 1}),
                        };
                        constexpr std::size_t per_row = 8;
                        for (std::size_t i = 0; i < N; i++)
                        {
                            if (i > 0 && i % per_row == 0)
                                ImGui::NewLine();
                            snprintf(buf, sizeof(buf), "##item_%zu", i);
                            const auto uv = v->texcoords_for_id(i);
                            ImGui::ImageButton(buf, (void*)&v->texture(), {TILE_SIZE[0]/2, TILE_SIZE[1]/2},
                                               { uv[3][0], uv[3][1] }, { uv[0][0], uv[0][1] });
                            if (ImGui::IsItemClicked())
                                _editor.floor().select_tile(v, (std::uint8_t)i);
                            else
                                click_event();
                            ImGui::SameLine();
                        }
                        ImGui::NewLine();
                    }
                    else
                    {
                        click_event();
                        add_tile_count();
                    }
                }
            }
        }
    }
}

void app::draw_fps()
{
    auto c1 = push_style_var(ImGuiStyleVar_FramePadding, {0, 0});
    auto c2 = push_style_var(ImGuiStyleVar_WindowPadding, {0, 0});
    auto c3 = push_style_var(ImGuiStyleVar_WindowBorderSize, 0);
    auto c4 = push_style_var(ImGuiStyleVar_WindowMinSize, {1, 1});
    auto c5 = push_style_var(ImGuiStyleVar_ScrollbarSize, 0);
    auto c6 = push_style_color(ImGuiCol_Text, {0, 1, 0, 1});

    char buf[16];
    const double dt = _frame_time > 1e-6f ? std::round(1/double(_frame_time)*10.)*.1 + 0.05 : 999;
    snprintf(buf, sizeof(buf), "%.1f FPS", dt);
    const ImVec2 size = ImGui::CalcTextSize(buf);

    ImGui::SetNextWindowPos({windowSize()[0] - size.x - 4, 3});
    ImGui::SetNextWindowSize(size);

    if (auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
        auto b = begin_window("framerate", ImGuiWindowFlags_(flags)))
    {
        ImGui::Text("%s", buf);
    }
}

void app::draw_cursor_coord()
{
    if (!_cursor_tile)
        return;

    auto c1 = push_style_var(ImGuiStyleVar_FramePadding, {0, 0});
    auto c2 = push_style_var(ImGuiStyleVar_WindowPadding, {0, 0});
    auto c3 = push_style_var(ImGuiStyleVar_WindowBorderSize, 0);
    auto c4 = push_style_var(ImGuiStyleVar_WindowMinSize, {1, 1});
    auto c5 = push_style_var(ImGuiStyleVar_ScrollbarSize, 0);
    auto c6 = push_style_color(ImGuiCol_Text, {.9f, .9f, .9f, 1});

    char buf[64];
    const auto coord = *_cursor_tile;
    const auto chunk = coord.chunk();
    const auto local = coord.local();
    snprintf(buf, sizeof(buf), "%hd:%hd - %hhu:%hhu", chunk.x, chunk.y, local.x, local.y);
    const auto size = ImGui::CalcTextSize(buf);

    ImGui::SetNextWindowPos({windowSize()[0]/2 - size.x/2, 3});
    ImGui::SetNextWindowSize(size);
    if (auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
        auto b = begin_window("tile-coord", ImGuiWindowFlags_(flags)))
    {
        ImGui::Text("%s", buf);
    }
}

} // namespace floormat
