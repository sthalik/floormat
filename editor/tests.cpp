#include "tests-private.hpp"
#include "app.hpp"
#include "floormat/events.hpp"
#include "imgui-raii.hpp"

namespace floormat {

using namespace floormat::tests;
using namespace floormat::imgui;

tests_data_::~tests_data_() noexcept = default;
tests_data_::tests_data_() = default;

tests_data::~tests_data() noexcept = default;
tests_data::tests_data() = default;

Pointer<tests_data_> tests_data_::make()
{
    return Pointer<tests_data>{InPlaceInit};
}

void app::tests_pre_update()
{
}

void app::tests_post_update()
{
}

bool app::tests_handle_mouse_click(const mouse_button_event& e)
{
    update_cursor_tile(cursor.pixel);
    auto& var = tests();

    if (e.button == mouse_button_left)
    {
        std::visit(overloaded {
            [](std::monostate) {},
            [&](path_test& t) {
                // todo
            }
        }, var);

        return true;
    }
    else if (e.button == mouse_button_right)
    {
        bool ret = false;
        std::visit(overloaded {
            [](std::monostate) {},
            [&](path_test& t) {
                ret = t.active;
                t = {};
            },
        }, var);
        return ret;
    }
    return false;
}

bool app::tests_handle_key(const key_event& e)
{
    return false;
}

bool app::tests_handle_mouse_move(const mouse_move_event& e)
{
    return false;
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

void app::draw_tests_pane(float main_menu_height)
{
}

void app::draw_tests_overlay()
{
}

} // namespace floormat
