#include "tests-private.hpp"
#include "app.hpp"
#include "floormat/events.hpp"
#include "imgui-raii.hpp"
#include <SDL_keycode.h>

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
    constexpr auto size = std::variant_size_v<tests::variant>;
    fm_assert(i < size);
    switch (i)
    {
    case 0: *this = std::monostate{}; break;
    case 1: *this = path_test{}; break;
    }
}

Pointer<tests_data_> tests_data_::make()
{
    return Pointer<tests_data>{InPlaceInit};
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

bool app::tests_handle_mouse_click(const mouse_button_event& e)
{
    update_cursor_tile(cursor.pixel);

    return std::visit(overloaded {
        [](std::monostate) { return false; },
        [&](base_test& x) { return x.handle_mouse_click(*this, e); }
    }, tests());
}

bool app::tests_handle_key(const key_event& e)
{
    switch (e.key)
    {
    case SDLK_ESCAPE:
        tests().switch_to(0);
        return true;
    default:
        return std::visit(overloaded {
            [](std::monostate) { return false; },
            [&](base_test& x) { return x.handle_key(*this, e); }
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

void app::draw_tests_pane()
{
    constexpr int selectable_flags = ImGuiSelectableFlags_SpanAvailWidth;
    for (auto [str, i] : tests_data::fields)
        if (ImGui::Selectable(str.data(), i == tests().index(), selectable_flags))
            tests().switch_to(i);
}

void app::draw_tests_overlay()
{
    std::visit(overloaded {
        [](std::monostate) {},
        [&](base_test& x) { return x.draw_overlay(*this); }
    }, tests());
}

} // namespace floormat
