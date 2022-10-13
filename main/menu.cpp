#include "app.hpp"
#include <Magnum/GL/Renderer.h>
#include <Magnum/ImGuiIntegration/Integration.h>

namespace floormat {

template<typename F>
struct raii_wrapper final
{
    raii_wrapper(F&& fn) : dtor{fn} {}
    [[no_unique_address]] F dtor;
    inline ~raii_wrapper() { dtor(); }
    raii_wrapper(const raii_wrapper<F>&) = delete;
    raii_wrapper<F>& operator=(const raii_wrapper<F>&) = delete;
};

constexpr inline const auto* imgui_name = "floormat editor";

[[nodiscard]] static auto gui_begin() {
    ImGui::Begin(imgui_name, nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_NoMove);
    return raii_wrapper{[]{ ImGui::End(); }};
}
[[nodiscard]] static auto begin_main_menu() {
    ImGui::BeginMainMenuBar();
    return raii_wrapper{[] { ImGui::EndMainMenuBar(); }};
}
[[nodiscard]] static auto begin_menu(const char* name, bool enabled = true) {
    ImGui::BeginMenu(name, enabled);
    return raii_wrapper{[] { ImGui::EndMenu(); }};
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

    auto b = gui_begin();
    draw_menu_bar();
}

void app::draw_menu_bar()
{
    auto bm = begin_main_menu();
    {
        auto m = begin_menu("File");
        ImGui::MenuItem("Open");
        ImGui::MenuItem("Recent");
        ImGui::Separator();
        ImGui::MenuItem("Save");
        ImGui::MenuItem("Save as...");
        ImGui::Separator();
        ImGui::MenuItem("Close");
    }
    {
        auto m = begin_menu("Mode");
        ImGui::MenuItem("Select", "F1");
        ImGui::MenuItem("Floors", "F2");
        ImGui::MenuItem("Walls", "F3");
        ImGui::MenuItem("Floors", "F4");
    }
}

} // namespace floormat
