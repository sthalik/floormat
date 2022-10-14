#include "app.hpp"
#include <Magnum/GL/Renderer.h>
#include <Magnum/ImGuiIntegration/Integration.h>

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

#if 0
[[nodiscard]] static raii_wrapper gui_begin() {
    using f = ImGuiWindowFlags_;
    int flags = 0;
    //flags |= ImGuiWindowFlags_AlwaysAutoResize;
    flags |= f::ImGuiWindowFlags_NoDecoration;
    if (ImGui::Begin(imgui_name, nullptr, flags))
        return {&ImGui::End; };
    else
        return {};
}
#endif
[[nodiscard]] static raii_wrapper begin_main_menu() {
    if (ImGui::BeginMainMenuBar())
        return raii_wrapper{&ImGui::EndMainMenuBar};
    else
        return {};
}
[[nodiscard]] static raii_wrapper begin_menu(const char* name, bool enabled = true) {
    if (ImGui::BeginMenu(name, enabled))
        return raii_wrapper{&ImGui::EndMenu};
    else
        return {};
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

    draw_menu_bar();
}

void app::draw_menu_bar()
{
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
            ImGui::MenuItem("Select", "F1", _editor_mode == editor_mode::select);
            ImGui::MenuItem("Floors", "F2", _editor_mode == editor_mode::floors);
            ImGui::MenuItem("Walls", "F3", _editor_mode == editor_mode::walls);
        }
    }
}

} // namespace floormat
