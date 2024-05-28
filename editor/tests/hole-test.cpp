#include "../tests-private.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "src/tile-constants.hpp"
#include "src/chunk-region.hpp"
#include "src/object.hpp"
#include "src/world.hpp"
#include "../app.hpp"
#include "../imgui-raii.hpp"
#include "floormat/main.hpp"
#include "src/critter.hpp"

namespace floormat::tests {
namespace {

using namespace floormat::imgui;

struct hole_test final : base_test
{
    ~hole_test() noexcept override = default;

    bool handle_key(app& a, const key_event& e, bool is_down) override;
    bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) override;
    bool handle_mouse_move(app& a, const mouse_move_event& e) override;
    void draw_overlay(app& a) override;
    void draw_ui(app& a, float menu_bar_height) override;
    void update_pre(app& a, const Ns& dt) override;
    void update_post(app&, const Ns&) override {}
};

bool hole_test::handle_key(app& a, const key_event& e, bool is_down)
{
    return false;
}

bool hole_test::handle_mouse_click(app& a, const mouse_button_event& e, bool is_down)
{
    return false;
}

bool hole_test::handle_mouse_move(app& a, const mouse_move_event& e)
{
    return false;
}

void hole_test::draw_overlay(app& a)
{
}

void hole_test::draw_ui(app& a, float menu_bar_height)
{
    const auto& m = a.main();
    const auto size_x = ImGui::GetWindowSize().x;
    const auto window_size = ImVec2{size_x, size_x};
    //const auto dpi = m.dpi_scale();
    constexpr auto igcf = ImGuiChildFlags_None;
    constexpr auto igwf = ImGuiWindowFlags_NoDecoration;

    ImGui::NewLine();

    char buf[32];

    ImGui::LabelText("##test-area", "Test area");

    ImGui::NewLine();
    if (auto b1 = imgui::begin_child("Test area"_s, window_size, igcf, igwf))
    {
        const auto& win = *ImGui::GetCurrentWindow();
        ImDrawList& draw = *win.DrawList;
    }
    ImGui::NewLine();

    const auto label_width = ImGui::CalcTextSize("MMMM").x;

    label_left("width", buf, label_width);
    ImGui::NewLine();

    label_left("height", buf, label_width);
    ImGui::NewLine();

    label_left("x", buf, label_width);
    ImGui::NewLine();

    label_left("y", buf, label_width);
    ImGui::NewLine();

    label_left("z", buf, label_width);
    ImGui::NewLine();
}

void hole_test::update_pre(app& a, const Ns& dt)
{
}

} // namespace

Pointer<base_test> tests_data::make_test_hole() { return Pointer<hole_test>{InPlaceInit}; }

} // namespace floormat::tests
