#include "tests-private.hpp"
#include "compat/safe-ptr.hpp"
#include "app.hpp"
#include "floormat/main.hpp"
#include "floormat/events.hpp"
#include "imgui-raii.hpp"
#define HAVE_LIBC 1
#include <SDL_keycode.h>

namespace floormat::tests {

static_assert(arraySize(tests_data::fields) == (size_t)Test::COUNT);

void label_left(StringView label, float width) // copy-pasted from inspect.cpp
{
    float x = ImGui::GetCursorPosX();
    ImGui::TextEx(label.data(), label.data() + label.size());
    ImGui::SameLine();
    ImGui::SetCursorPosX(x + width + ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(-1);
}

Pointer<base_test> tests_data::make_test_none() { return {}; }

} // namespace floormat::tests

namespace floormat {

using namespace floormat::tests;

tests_data_::~tests_data_() noexcept = default;
tests_data_::tests_data_() = default;

tests_data::~tests_data() noexcept = default;

base_test::~base_test() noexcept = default;
base_test::base_test() = default;

using namespace floormat::imgui;

void tests_data::switch_to(Test i)
{
    fm_assert((size_t)i < arraySize(fields));
    current_index = Test::none;
    current_test = make_test_none();
    switch (i)
    {
    default: break;
    case Test::none: current_test = make_test_none(); break;
    case Test::path: current_test = make_test_path(); break;
    case Test::raycast: current_test = make_test_raycast(); break;
    }
    if (current_test)
        current_index = i;
}

safe_ptr<tests_data_> tests_data_::make()
{
    return safe_ptr<tests_data_>{new tests_data};
}

void app::tests_pre_update()
{
    if (auto& x = tests().current_test)
        x->update_pre(*this);
}

void app::tests_post_update()
{
    if (auto& x = tests().current_test)
        x->update_post(*this);
}

bool app::tests_handle_mouse_click(const mouse_button_event& e, bool is_down)
{
    update_cursor_tile(cursor.pixel);

    if (auto& x = tests().current_test)
        return x->handle_mouse_click(*this, e, is_down);
    else
        return false;
}

bool app::tests_handle_key(const key_event& e, bool is_down)
{
    if (auto& x = tests().current_test)
        return x->handle_key(*this, e, is_down);
    else
        return false;
}

bool app::tests_handle_mouse_move(const mouse_move_event& e)
{
    if (auto& x = tests().current_test)
        return x->handle_mouse_move(*this, e);
    else
        return false;
}

tests_data& app::tests()
{
    return static_cast<tests_data&>(*_tests);
}

void app::tests_reset_mode()
{
    tests().switch_to(Test::none);
}

void app::draw_tests_pane(float width)
{
    ImGui::SeparatorText("Functional tests");
    auto& t = tests();

    constexpr int selectable_flags = ImGuiSelectableFlags_SpanAvailWidth;

    for (auto [str, id, ctor] : tests_data::fields)
        if (ImGui::Selectable(str.data(), id == t.current_index, selectable_flags))
            if (t.current_index != id)
                t.switch_to(id);

    if (t.current_test)
    {
        auto dpi = M->dpi_scale();
        ImGui::NewLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2*dpi.y());
        auto b = push_id("###test-data");
        t.current_test->draw_ui(*this, width);
    }
}

void app::draw_tests_overlay()
{
    if (auto& x = tests().current_test)
        x->draw_overlay(*this);
}

} // namespace floormat
