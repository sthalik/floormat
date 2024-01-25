#include "tests-private.hpp"
#include "compat/safe-ptr.hpp"
#include "app.hpp"
#include "floormat/main.hpp"
#include "floormat/events.hpp"
#include "imgui-raii.hpp"
#define HAVE_LIBC 1
#include <SDL_keycode.h>

namespace floormat::tests {

void label_left(StringView label, float width)
{
    float x = ImGui::GetCursorPosX();
    ImGui::TextEx(label.data(), label.data() + label.size());
    ImGui::SameLine();
    ImGui::SetCursorPosX(x + width + ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(-1);
}

} // namespace floormat::tests

namespace floormat {

using namespace floormat::tests;

tests_data_::~tests_data_() noexcept = default;
tests_data_::tests_data_() = default;

tests_data::~tests_data() noexcept = default;
tests_data::tests_data() = default;

base_test::~base_test() noexcept = default;
base_test::base_test() = default;

using namespace floormat::imgui;

void tests_data::switch_to(size_t i)
{
    fm_assert(i < std::size(fields));
    const auto& [str, index, ctor] = fields[i];
    *this = ctor();
}

safe_ptr<tests_data_> tests_data_::make()
{
    return safe_ptr<tests_data_>{new tests_data};
}

void app::tests_pre_update()
{
    std::visit(overloaded {
        [](std::monostate) {},
        [&](base_test& x) { return x.update_pre(*this); }
    }, tests());
}

void app::tests_post_update()
{
    std::visit(overloaded {
        [](std::monostate) {},
        [&](base_test& x) { return x.update_post(*this); }
    }, tests());
}

bool app::tests_handle_mouse_click(const mouse_button_event& e, bool is_down)
{
    update_cursor_tile(cursor.pixel);

    return std::visit(overloaded {
        [](std::monostate) { return false; },
        [&](base_test& x) { return x.handle_mouse_click(*this, e, is_down); }
    }, tests());
}

bool app::tests_handle_key(const key_event& e, bool is_down)
{
    switch (e.key)
    {
    case SDLK_ESCAPE:
        tests().switch_to(0);
        return true;
    default:
        return std::visit(overloaded {
            [](std::monostate) { return false; },
            [&](base_test& x) { return x.handle_key(*this, e, is_down); }
        }, tests());
    }
}

bool app::tests_handle_mouse_move(const mouse_move_event& e)
{
    return std::visit(overloaded {
        [](std::monostate) { return false; },
        [&](base_test& x) { return x.handle_mouse_move(*this, e); }
    }, tests());
}

tests_data& app::tests()
{
    return static_cast<tests_data&>(*_tests);
}

void app::tests_reset_mode()
{
    if (!std::holds_alternative<std::monostate>(tests()))
        tests() = std::monostate{};
}

void app::draw_tests_pane(float width)
{
    ImGui::SeparatorText("Functional tests");

    constexpr int selectable_flags = ImGuiSelectableFlags_SpanAvailWidth;
    for (auto [str, i, ctor] : tests_data::fields)
        if (ImGui::Selectable(str.data(), i == tests().index(), selectable_flags))
            tests().switch_to(i);

    std::visit(overloaded {
        [](std::monostate) {},
        [&](base_test& x) {
            auto dpi = M->dpi_scale();
            ImGui::NewLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2*dpi.y());
            auto b = push_id("###test-data");
            return x.draw_ui(*this, width);
        }
    }, tests());
}

void app::draw_tests_overlay()
{
    std::visit(overloaded {
        [](std::monostate) {},
        [&](base_test& x) {
          return x.draw_overlay(*this);
        }
    }, tests());
}

} // namespace floormat
