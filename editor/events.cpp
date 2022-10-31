#include "app.hpp"

#include "floormat/main.hpp"
#include "floormat/events.hpp"
#include "src/world.hpp"

#include <utility>

#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/ImGuiIntegration/Context.hpp>

namespace floormat {

void app::on_focus_in() noexcept {}
void app::on_mouse_enter() noexcept {}
void app::on_any_event(const any_event&) noexcept {}

#define accessor(type, name) \
    type m_##name = {}; auto name() const noexcept { return m_##name; }

void app::on_mouse_move(const mouse_move_event& event) noexcept
{
    struct {
        accessor(Vector2i, position)
    } e = {event.position};

    cursor.in_imgui = _imgui.handleMouseMoveEvent(e);
    update_cursor_tile(event.position);
    do_mouse_move();
}

void app::on_mouse_up_down(const mouse_button_event& event, bool is_down) noexcept
{
    enum class Button : std::underlying_type_t<mouse_button> {
        Left = mouse_button_left,
        Right = mouse_button_right,
        Middle = mouse_button_middle,
    };
    struct ev {
        using Button = Button;
        accessor(Vector2i, position)
        accessor(Button, button)
    } e = {event.position, Button(event.button)};

    if (cursor.in_imgui = is_down
                          ? _imgui.handleMousePressEvent(e)
                          : _imgui.handleMouseReleaseEvent(e);
        !cursor.in_imgui)
    {
        update_cursor_tile(event.position);
        if (cursor.tile)
            do_mouse_up_down(event.button, is_down);
    }
}

void app::on_mouse_scroll(const mouse_scroll_event& event) noexcept
{
    struct {
        accessor(Vector2, offset)
        accessor(Vector2i, position)
    } e = {event.offset, event.position};
    _imgui.handleMouseScrollEvent(e);
}

void app::on_key_up_down(const key_event& event, bool is_down) noexcept
{
    using KeyEvent = Platform::Sdl2Application::KeyEvent;
    struct Ev final {
        using Key = KeyEvent::Key;
        using Modifier = KeyEvent::Modifier;
        using Modifiers = KeyEvent::Modifiers;
        accessor(Key, key)
        accessor(Modifiers, modifiers)
    } e = {Ev::Key(event.key), Ev::Modifier(event.mods)};

    if (!(is_down ? _imgui.handleKeyPressEvent(e) : _imgui.handleKeyReleaseEvent(e)))
    {
        // todo put it into a separate function
        const key x = fm_begin(switch (event.key) {
                               default:             return key::COUNT;
                               case SDLK_w:         return key::camera_up;
                               case SDLK_a:         return key::camera_left;
                               case SDLK_s:         return key::camera_down;
                               case SDLK_d:         return key::camera_right;
                               case SDLK_HOME:      return key::camera_reset;
                               case SDLK_r:         return key::rotate_tile;
                               case SDLK_1:         return key::mode_select;
                               case SDLK_2:         return key::mode_floor;
                               case SDLK_3:         return key::mode_walls;
                               case SDLK_F5:        return key::quicksave;
                               case SDLK_F9:        return key::quickload;
                               case SDLK_ESCAPE:    return key::quit;
        });
        if (x != key::COUNT)
        {
            keys[x] = is_down;
            keys_repeat[x] = is_down ? event.is_repeated : false;
        }
    }
    else
    {
        keys = {};
        keys_repeat = {};
    }
}

void app::on_text_input_event(const text_input_event& event) noexcept
{
    struct {
        accessor(Containers::StringView, text)
    } e = {event.text};
    if (_imgui.handleTextInputEvent(e))
    {
        keys = {};
        keys_repeat = {};
    }
}

void app::on_viewport_event(const Math::Vector2<int>& size) noexcept
{
    init_imgui(size);
}

void app::on_focus_out() noexcept
{
    update_cursor_tile(std::nullopt);
    keys = {};
    keys_repeat = {};
}

void app::on_mouse_leave() noexcept
{
    update_cursor_tile(std::nullopt);
}

} // namespace floormat
