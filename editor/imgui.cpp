#include "app.hpp"
#include "main/floormat-main.hpp"
#include <Magnum/GL/Renderer.h>
#include "imgui-raii.hpp"
#include <Magnum/ImGuiIntegration/Context.h>

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
    ImGui::GetIO().IniFilename = nullptr;
    _imgui.newFrame();
    ImGui::StyleColorsDark(&ImGui::GetStyle());

    const float main_menu_height = draw_main_menu();
    draw_editor_pane(_editor.floor(), main_menu_height);
    draw_fps();
    draw_cursor_tile();
    ImGui::EndFrame();
}

void app::draw_editor_pane(tile_editor& type, float main_menu_height)
{
    const auto window_size = M->window_size();

    constexpr
    Color4 color_perm_selected{1, 1, 1, .7f},
           color_selected{1, 0.843f, 0, .8f},
           color_hover{0, .8f, 1, .7f};

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
    tile_editor* const ed = _editor.current();

    if (main_menu_height > 0)
    {
        ImGui::SetNextWindowPos({0, main_menu_height+style.WindowPadding.y});
        ImGui::SetNextFrameWantCaptureKeyboard(false);
        ImGui::SetNextWindowSize({420, window_size[1] - main_menu_height - style.WindowPadding.y});
        if (const auto flags = ImGuiWindowFlags_(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            auto b = begin_window({}, flags))
        {
            const float window_width = ImGui::GetWindowWidth() - 32;

            char buf[128];
            //ImGui::SetNextWindowBgAlpha(.2f);

            if (auto b = begin_list_box("##atlases", {-FLT_MIN, -1}))
            {
                for (const auto& [k, v] : type)
                {
                    ///const auto& k_ = k;
                    const auto& v_ = v;
                    const auto click_event = [&] {
                        if (ed)
                        {
                            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                                ed->select_tile_permutation(v_);
                            else if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
                                ed->clear_selection();
                        }
                    };
                    const auto do_caption = [&] {
                        if (ed)
                        {
                            click_event();
                            if (ed->is_atlas_selected(v_))
                            {
                                ImGui::SameLine();
                                ImGui::Text(" (selected)");
                            }
                        }
                      {
                          snprintf(buf, sizeof(buf), "%zu", (std::size_t)v_->num_tiles());
                          ImGui::SameLine(window_width - ImGui::CalcTextSize(buf).x - style.FramePadding.x - 4);
                          ImGui::Text("%s", buf);
                      }
                    };
                    const auto N = v->num_tiles();
                    if (const auto flags = ImGuiTreeNodeFlags_(ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Framed);
                        auto b = tree_node(k.data(), flags))
                    {
                        do_caption();
                        [[maybe_unused]] const raii_wrapper vars[] = {
                            push_style_var(ImGuiStyleVar_FramePadding, {2, 2}),
                            push_style_color(ImGuiCol_ButtonHovered, color_hover),
                        };
                        const bool perm_selected = ed ? ed->is_permutation_selected(v) : false;
                        constexpr std::size_t per_row = 8;
                        for (std::size_t i = 0; i < N; i++)
                        {
                            const bool selected = ed ? ed->is_tile_selected(v, i) : false;

                            if (i > 0 && i % per_row == 0)
                                ImGui::NewLine();

                            [[maybe_unused]] const raii_wrapper vars[] = {
                                selected ? push_style_color(ImGuiCol_Button, color_selected) : raii_wrapper{},
                                selected ? push_style_color(ImGuiCol_ButtonHovered, color_selected) : raii_wrapper{},
                                perm_selected ? push_style_color(ImGuiCol_Button, color_perm_selected) : raii_wrapper{},
                                perm_selected ? push_style_color(ImGuiCol_ButtonHovered, color_perm_selected) : raii_wrapper{},
                            };

                            snprintf(buf, sizeof(buf), "##item_%zu", i);
                            const auto uv = v->texcoords_for_id(i);
                            ImGui::ImageButton(buf, (void*)&v->texture(), {TILE_SIZE[0]/2, TILE_SIZE[1]/2},
                                               { uv[3][0], uv[3][1] }, { uv[0][0], uv[0][1] });
                            if (ed)
                            {
                                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                                    ed->select_tile(v, i);
                                else
                                    click_event();
                            }
                            ImGui::SameLine();
                        }
                        ImGui::NewLine();
                    }
                    else
                        do_caption();
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

    const auto frame_time = M->smoothed_dt();
    char buf[16];
    const double hz = frame_time > 1e-6f ? (int)std::round(10./(double)frame_time + .05) * .1 : 9999;
    snprintf(buf, sizeof(buf), "%.1f FPS", hz);
    const ImVec2 size = ImGui::CalcTextSize(buf);

    ImGui::SetNextWindowPos({M->window_size()[0] - size.x - 4, 3});
    ImGui::SetNextWindowSize(size);

    if (auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
        auto b = begin_window("framerate", ImGuiWindowFlags_(flags)))
    {
        ImGui::Text("%s", buf);
    }
}

void app::draw_cursor_tile_coord()
{
    if (!cursor.tile)
        return;

    auto c1 = push_style_var(ImGuiStyleVar_FramePadding, {0, 0});
    auto c2 = push_style_var(ImGuiStyleVar_WindowPadding, {0, 0});
    auto c3 = push_style_var(ImGuiStyleVar_WindowBorderSize, 0);
    auto c4 = push_style_var(ImGuiStyleVar_WindowMinSize, {1, 1});
    auto c5 = push_style_var(ImGuiStyleVar_ScrollbarSize, 0);
    auto c6 = push_style_color(ImGuiCol_Text, {.9f, .9f, .9f, 1});

    char buf[64];
    const auto coord = *cursor.tile;
    const auto chunk = coord.chunk();
    const auto local = coord.local();
    snprintf(buf, sizeof(buf), "%hd:%hd - %hhu:%hhu", chunk.x, chunk.y, local.x, local.y);
    const auto size = ImGui::CalcTextSize(buf);
    const auto window_size = M->window_size();

    ImGui::SetNextWindowPos({window_size[0]*.5f - size.x/2, 3});
    ImGui::SetNextWindowSize(size);
    if (auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
        auto b = begin_window("tile-tile", ImGuiWindowFlags_(flags)))
    {
        ImGui::Text("%s", buf);
    }
}

} // namespace floormat
