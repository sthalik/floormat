#include "app.hpp"

#include "floormat/main.hpp"
#include "floormat/events.hpp"
#include "src/world.hpp"
#include "keys.hpp"

#include <utility>

#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/ImGuiIntegration/Context.hpp>

#include <SDL_keycode.h>
#include <SDL_events.h>

namespace floormat {

void app::on_focus_in() noexcept {}
void app::on_mouse_enter() noexcept {}
void app::on_any_event(const any_event&) noexcept {}

#define accessor(type, name) \
    type m_##name = {}; auto name() const noexcept { return m_##name; }

static constexpr int fixup_mods_(int mods, int value, int mask)
{
    return !!(mods & mask) * value;
}

static constexpr int fixup_mods(int mods)
{
    int ret = 0;
    ret |= fixup_mods_(mods, kmod_ctrl,  KMOD_CTRL);
    ret |= fixup_mods_(mods, kmod_shift, KMOD_SHIFT);
    ret |= fixup_mods_(mods, kmod_alt,   KMOD_ALT);
    ret |= fixup_mods_(mods, kmod_super, KMOD_GUI);
    return ret;
}

void app::clear_keys(key min_inclusive, key max_exclusive)
{
    using key_type = decltype(keys)::value_type;
    for (key_type i = key_type(min_inclusive); i < key_type(max_exclusive); i++)
    {
        const auto idx = key(i);
        keys[idx] = false;
        key_modifiers[i] = kmod_none;
    }
}

void app::clear_keys()
{
    keys.reset();
    key_modifiers = {};
}

void app::on_mouse_move(const mouse_move_event& event) noexcept
{
    struct {
        accessor(Vector2i, position)
    } e = {event.position};

    cursor.in_imgui = _imgui.handleMouseMoveEvent(e);
    update_cursor_tile(event.position);
    do_mouse_move(fixup_mods(event.mods));
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

    if (!(cursor.in_imgui = is_down ? _imgui.handleMousePressEvent(e) : _imgui.handleMouseReleaseEvent(e)))
        do_mouse_up_down(event.button, is_down, fixup_mods(event.mods));
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

    [[maybe_unused]] constexpr int CTRL  = kmod_ctrl;
    [[maybe_unused]] constexpr int SHIFT = kmod_shift;
    [[maybe_unused]] constexpr int ALT   = kmod_alt;
    [[maybe_unused]] constexpr int SUPER = kmod_super;

    const auto mods = fixup_mods(event.mods);

    const key x = fm_begin(
        int k = event.key | mods;
        constexpr kmod list[] = { kmod_none, kmod_super, kmod_alt, kmod_shift, kmod_ctrl, };
        int last = ~0;
        for (kmod mod1 : list)
        {
            k &= ~mod1;
            for (int k2 = k; kmod mod2 : list)
            {
                k2 &= ~mod2;
                if (k2 == last)
                    continue;
                last = k2;
                switch (k2)
                {
                case SDLK_w:        return key_camera_up;
                case SDLK_a:        return key_camera_left;
                case SDLK_s:        return key_camera_down;
                case SDLK_d:        return key_camera_right;
                case SDLK_HOME:     return key_camera_reset;
                case SDLK_r:        return key_rotate_tile;
                case SDLK_1:        return key_mode_none;
                case SDLK_2:        return key_mode_floor;
                case SDLK_3:        return key_mode_walls;
                case SDLK_F5:       return key_quicksave;
                case SDLK_F9:       return key_quickload;
                case SDLK_q | CTRL: return key_quit;
                default: break;
                }
            }
        }


        return key_COUNT;
    );

    if (x == key_COUNT)
        void();
    else if (x >= key_NO_REPEAT)
        is_down && !event.is_repeated ? do_key(x, mods) : void();
    else if (is_down ? _imgui.handleKeyPressEvent(e) : _imgui.handleKeyReleaseEvent(e))
        clear_non_global_keys();
    else {
        keys[x] = is_down;
        key_modifiers[std::size_t(x)] = mods;
    }
}

void app::on_text_input_event(const text_input_event& event) noexcept
{
    struct {
        accessor(Containers::StringView, text)
    } e = {event.text};
    if (_imgui.handleTextInputEvent(e))
        clear_non_global_keys();
}

void app::on_viewport_event(const Math::Vector2<int>& size) noexcept
{
    init_imgui(size);
}

void app::on_focus_out() noexcept
{
    update_cursor_tile(std::nullopt);
    clear_keys();
}

void app::on_mouse_leave() noexcept
{
    update_cursor_tile(std::nullopt);
}

void app::do_key(floormat::key k)
{
    do_key(k, get_key_modifiers());
}

int app::get_key_modifiers()
{
    return fixup_mods(SDL_GetModState());
}

} // namespace floormat
