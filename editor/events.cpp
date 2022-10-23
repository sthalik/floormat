#include "app.hpp"
#include "main/floormat-main.hpp"
#include "main/floormat-events.hpp"
#include "src/world.hpp"
#include <utility>
#include <Magnum/ImGuiIntegration/Context.hpp>

namespace floormat {

#define accessor(type, name) \
    type m_##name = {}; auto name() const noexcept { return m_##name; }

bool app::on_mouse_move(const mouse_move_event& event) noexcept
{
    struct {
        accessor(Vector2i, position)
    } e = {event.position};

    cursor.in_imgui = _imgui.handleMouseMoveEvent(e);

    if (!cursor.in_imgui)
        cursor.pixel = event.position;
    else
        cursor.pixel = std::nullopt;
    recalc_cursor_tile();

    if (cursor.tile)
        _editor.on_mouse_move(M->world(), *cursor.tile);

    return true;
}

bool app::on_mouse_up_down(const mouse_button_event& event, bool is_down) noexcept
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

    if (!(cursor.in_imgui = is_down
                            ? _imgui.handleMousePressEvent(e)
                            : _imgui.handleMouseReleaseEvent(e)))
    {
        cursor.pixel = event.position;
        recalc_cursor_tile();
        if (cursor.tile)
        {
            if (event.button == mouse_button_left && is_down)
                _editor.on_click(M->world(), *cursor.tile);
            else
                _editor.on_release();
        }
    }

    return true;
}

bool app::on_mouse_scroll(const mouse_scroll_event& event) noexcept
{
    struct {
        accessor(Vector2, offset)
        accessor(Vector2i, position)
    } e = {event.offset, event.position};
    _imgui.handleMouseScrollEvent(e);
    return true;
}

bool app::on_key_up_down(const floormat::key_event& event, bool is_down) noexcept
{
    enum Modifier : std::underlying_type_t<SDL_Keymod> {
        None = KMOD_NONE,
        Ctrl = KMOD_CTRL,
        Shift = KMOD_SHIFT,
        Alt = KMOD_ALT,
        Super = KMOD_GUI,
    };
    struct {
        accessor(SDL_Keymod, modifiers)
    } e = {};
    if (!(is_down ? _imgui.handleKeyPressEvent(e) : _imgui.handleKeyReleaseEvent(e)))
    {
        const key x = fm_begin(switch (event.key) {
                               default:            return key::COUNT;
                               case SDLK_w:        return key::camera_up;
                               case SDLK_a:        return key::camera_left;
                               case SDLK_s:        return key::camera_down;
                               case SDLK_d:        return key::camera_right;
                               case SDLK_HOME:     return key::camera_reset;
                               case SDLK_r:        return key::rotate_tile;
                               case SDLK_F5:       return key::quicksave;
                               case SDLK_F9:       return key::quickload;
                               case SDLK_ESCAPE:   return key::quit;
        });
        if (x != key::COUNT)
            _keys[x] = is_down && !event.is_repeated;
    }
    return true;
}

bool app::on_text_input_event(const floormat::text_input_event& event) noexcept
{
    struct {
        accessor(Containers::StringView, text)
    } e = {event.text};
    if (_imgui.handleTextInputEvent(e))
        _keys = {};
    return true;
}

} // namespace floormat
