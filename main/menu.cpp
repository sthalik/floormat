#include "app.hpp"
#include <Magnum/GL/Renderer.h>
#include <Magnum/ImGuiIntegration/Integration.h>
#include <Magnum/Math/Color.h>

namespace floormat {

constexpr inline auto noop = []{};

struct raii_wrapper final
{
    using F = void(*)(void);
    raii_wrapper(F fn) : dtor{fn} {}
    raii_wrapper() = default;
    ~raii_wrapper() { if (dtor) dtor(); }
    raii_wrapper(const raii_wrapper&) = delete;
    raii_wrapper& operator=(const raii_wrapper&) = delete;
    raii_wrapper& operator=(raii_wrapper&&) = delete;
    raii_wrapper(raii_wrapper&& other) noexcept : dtor{other.dtor} { other.dtor = nullptr; }
    inline operator bool() const noexcept { return dtor != nullptr; }

    F dtor = nullptr;
};

constexpr inline const auto* imgui_name = "floormat editor";

[[nodiscard]] static raii_wrapper begin_window(int flags = 0) {
    if (ImGui::Begin(imgui_name, nullptr, flags))
        return {&ImGui::End};
    else
        return {};
}

[[nodiscard]] static raii_wrapper begin_main_menu() {
    if (ImGui::BeginMainMenuBar())
        return {&ImGui::EndMainMenuBar};
    else
        return {};
}
[[nodiscard]] static raii_wrapper begin_menu(const char* name, bool enabled = true) {
    if (ImGui::BeginMenu(name, enabled))
        return {&ImGui::EndMenu};
    else
        return {};
}

[[nodiscard]] static raii_wrapper begin_list_box(const char* name, ImVec2 size = {}) {
    if (ImGui::BeginListBox(name, size))
        return {&ImGui::EndListBox};
    else
        return {};
}

[[nodiscard]] static raii_wrapper tree_node(const char* name) {
    if (ImGui::TreeNode(name))
        return {&ImGui::TreePop};
    else
        return {};
}

[[nodiscard]] static raii_wrapper push_style_var(ImGuiStyleVar_ var, Vector2 value)
{
    ImGui::PushStyleVar(var, {value[0], value[1]});
    return {[]{ ImGui::PopStyleVar(); }};
}

[[nodiscard]] static raii_wrapper push_style_var(ImGuiStyleVar_ var, float value)
{
    ImGui::PushStyleVar(var, value);
    return {[]{ ImGui::PopStyleVar(); }};
}

[[nodiscard]] static raii_wrapper push_style_color(ImGuiCol_ var, const Color4& value)
{
    ImGui::PushStyleColor(var, {value[0], value[1], value[2], value[3]});
    return {[]{ ImGui::PopStyleColor(); }};
}

void app::setup_menu()
{
    GL::Renderer::setBlendEquation(GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add);
    GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha, GL::Renderer::BlendFunction::OneMinusSourceAlpha);
}

void app::display_menu()
{
    GL::Renderer::enable(GL::Renderer::Feature::Blending);
    GL::Renderer::enable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);
    _imgui.drawFrame();
    GL::Renderer::enable(GL::Renderer::Feature::ScissorTest);
}

void app::draw_menu()
{
    _imgui.newFrame();

    if (ImGui::GetIO().WantTextInput && !isTextInputActive())
        startTextInput();
    else if (!ImGui::GetIO().WantTextInput && isTextInputActive())
        stopTextInput();

    auto& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.WindowPadding = {8, 8};
    style.WindowBorderSize = {};
    style.Colors[ImGuiCol_WindowBg] = {0, 0, 0, .5};
    style.Colors[ImGuiCol_FrameBg] = {0, 0, 0, 0};

    ImVec2 main_menu_pos;

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
            ImGui::MenuItem("Floors", "F2", _editor.mode() == editor_mode::floors);
            ImGui::MenuItem("Walls", "F3", _editor.mode() == editor_mode::walls);
        }
        main_menu_pos = ImGui::GetContentRegionMax();
    }
    if (main_menu_pos.y > 0)
    {
        ImGui::SetNextWindowPos({0, main_menu_pos.y+style.WindowPadding.y});
        ImGui::SetNextFrameWantCaptureKeyboard(false);
        ImGui::SetNextWindowSize({450, windowSize()[1] - main_menu_pos.y - style.WindowPadding.y*2});
        if (auto b = begin_window(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::Text("Items:");
            //ImGui::SetNextWindowBgAlpha(.2f);

            if (auto b = begin_list_box("##tiles", {-FLT_MIN, -1}))
            {
                for (const auto& [k, v] : _editor.floors())
                {
                    const std::size_t N = v->num_tiles().product();
                    if (auto b = tree_node(k.data()))
                    {
                        auto c = push_style_var(ImGuiStyleVar_FramePadding, {1, 1});
                        auto c2 = push_style_var(ImGuiStyleVar_FrameBorderSize, 3);
                        auto c3 = push_style_color(ImGuiCol_Button, {1, 1, 1, 1});
                        constexpr std::size_t per_row = 8;
                        for (std::size_t i = 0; i < N; i++)
                        {
                            if (i > 0 && i % per_row == 0)
                                ImGui::NewLine();
                            char buf[64];
                            sprintf(buf, "##item_%zu", i);
                            const auto uv = v->texcoords_for_id(i);
                            ImGui::ImageButton(buf, (void*)&v->texture(), {TILE_SIZE[0]/2, TILE_SIZE[1]/2},
                                               { uv[3][0], uv[3][1] }, { uv[0][0], uv[0][1] });
                            if (ImGui::IsItemClicked())
                            {
                                printf("clicked %s %zu\n", buf+2, i);
                                fflush(stdout);
                            }
                            ImGui::SameLine();
                        }
                        ImGui::NewLine();
                    }
                }
            }
        }
    }
}

} // namespace floormat
