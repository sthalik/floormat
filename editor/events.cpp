#include "app.hpp"

#include "floormat/main.hpp"
#include "floormat/events.hpp"
#include "main/sdl-fwd.inl"
#include "src/world.hpp"
#include "keys.hpp"
#include "editor.hpp"
#include "compat/enum-bitset.hpp"
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StructuredBindings.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/ImGuiIntegration/Context.hpp>

namespace floormat {

namespace {

constexpr int fixup_mods_(int mods, int value, int mask)
{
    return !!(mods & mask) * value;
}

constexpr int fixup_mods(int mods)
{
    int ret = 0;
    ret |= fixup_mods_(mods, kmod_ctrl,  KMOD_CTRL);
    ret |= fixup_mods_(mods, kmod_shift, KMOD_SHIFT);
    ret |= fixup_mods_(mods, kmod_alt,   KMOD_ALT);
    ret |= fixup_mods_(mods, kmod_super, KMOD_GUI);
    return ret;
}

} // namespace


using PointerButtons = Platform::Sdl2Application::Pointer;
using PointerEvent = Platform::Sdl2Application::PointerEvent;
using PointerMoveEvent = Platform::Sdl2Application::PointerMoveEvent;

void app::on_focus_in() noexcept {}
void app::on_mouse_enter() noexcept {}
void app::on_any_event(const any_event&) noexcept {}

#define accessor(type, name) \
    type m_##name = {}; auto name() const noexcept { return m_##name; }

bool app::do_imgui_key(const sdl2::EvKey& ev, bool is_down)
{
    if (is_down)
        return _imgui->handleKeyPressEvent(ev.val);
    else
        return _imgui->handleKeyReleaseEvent(ev.val);
}

bool app::do_imgui_click(const sdl2::EvClick& ev, bool is_down)
{
    if (is_down)
        return _imgui->handlePointerPressEvent(ev.val);
    else
        return _imgui->handlePointerReleaseEvent(ev.val);
}

bool app::do_tests_key(const key_event& ev, bool is_down)
{
    bool ret = _editor->mode() == editor_mode::tests;
    if (ret)
        return tests_handle_key(ev, is_down);
    return ret;
}

void app::clear_keys(key min_inclusive, key max_exclusive)
{
    auto& keys = *keys_;
    using key_type = std::decay_t<decltype(keys)>::value_type;
    for (auto i = key_type(min_inclusive); i < key_type(max_exclusive); i++)
    {
        const auto idx = key(i);
        keys[idx] = false;
        key_modifiers.data[i] = kmod_none;
    }
}

void app::clear_keys()
{
    keys_->reset();
    key_modifiers = {};
}

void app::on_mouse_move(const mouse_move_event& event, const sdl2::EvMove& ev) noexcept
{
    do
    {
        cursor.in_imgui = _imgui->handlePointerMoveEvent(ev.val);
        if (cursor.in_imgui)
            break;
        if (_editor->mode() == editor_mode::tests)
        {
            (void)tests_handle_mouse_move(event);
            break;
        }
    }
    while (false);

    update_cursor_tile(Vector2i(event.position));
    do_mouse_move(fixup_mods(event.mods));
}

void app::on_mouse_up_down(const mouse_button_event& event, bool is_down, const sdl2::EvClick& ev) noexcept
{
    const auto p = Vector2i(event.position);

    if (!(p >= Vector2i{} && p < M->window_size()))
        return;

    do
    {
        cursor.in_imgui = do_imgui_click(ev, is_down);
        if (cursor.in_imgui)
            break;
        if (_editor->mode() == editor_mode::tests)
            if (tests_handle_mouse_click(event, is_down))
                break;
        do_mouse_up_down(event.button, is_down, fixup_mods(event.mods));
    }
    while(false);
}

void app::on_mouse_scroll(const mouse_scroll_event& event, const sdl2::EvScroll& ev) noexcept
{
    const auto p = Vector2i(event.position);

    do
    {
        if (p >= Vector2i() && p < M->window_size())
            break;
        cursor.in_imgui = _imgui->handleScrollEvent(ev.val);
        if (cursor.in_imgui)
            break;
        do_mouse_scroll((int)ev.val.offset()[1]);
    }
    while (false);
}

auto app::resolve_keybinding(int k_, int mods_) -> Pair<key, int>
{
    [[maybe_unused]] constexpr int CTRL  = kmod_ctrl;
    [[maybe_unused]] constexpr int SHIFT = kmod_shift;
    [[maybe_unused]] constexpr int ALT   = kmod_alt;
    [[maybe_unused]] constexpr int SUPER = kmod_super;

    switch (k_)
    {
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
    case SDLK_LCTRL:
    case SDLK_RCTRL:
    case SDLK_LALT:
    case SDLK_RALT:
    case SDLK_LGUI:
    case SDLK_RGUI:
        return { key_noop, kmod_mask };
    default:
        break;
    }

    const int k = k_ | fixup_mods(mods_);
    constexpr kmod list[] = { kmod_none, kmod_super, kmod_alt, kmod_shift, kmod_ctrl, };
    int last = ~0;
    for (int k1 = k; kmod mod1 : list)
    {
        k1 &= ~mod1;
        for (int k2 = k1; kmod mod2 : list)
        {
            k2 &= ~mod2;
            if (k2 == last)
                continue;
            last = k2;
            auto mods = k2 & kmod_mask;
            auto ret = [=]
            {
                switch (k2)
                {
                default:            return key_noop;
                case SDLK_w:        return key_camera_up;
                case SDLK_a:        return key_camera_left;
                case SDLK_s:        return key_camera_down;
                case SDLK_d:        return key_camera_right;
                case SDLK_HOME:     return key_camera_reset;
                case SDLK_r:        return key_rotate_tile;
                case SDLK_F2:       return key_emit_timestamp;
                case SDLK_1:        return key_mode_none;
                case SDLK_2:        return key_mode_floor;
                case SDLK_3:        return key_mode_walls;
                case SDLK_4:        return key_mode_scenery;
                case SDLK_5:        return key_mode_vobj;
                // for things like:
                // - detect collisions with a line placed using the cursor (can be diagonal)
                // - make charactere pathfind somewhere
                // - make character walk around waypoints
                case SDLK_6:        return key_mode_tests;
                case SDLK_c | ALT:  return key_render_collision_boxes;
                case SDLK_l | ALT:  return key_render_clickables;
                case SDLK_v | ALT:  return key_render_vobjs;
                case SDLK_t:        return key_render_all_z_levels;
                case SDLK_F5:       return key_quicksave;
                case SDLK_F9:       return key_quickload;
                case SDLK_q | CTRL: return key_quit;
                case SDLK_n | CTRL: return key_new_file;
                case SDLK_ESCAPE:   return key_escape;
                case SDLK_LEFT:     return key_left;
                case SDLK_RIGHT:    return key_right;
                case SDLK_UP:       return key_up;
                case SDLK_DOWN:     return key_down;
                }
            }();
            if (ret == key_noop)
                continue;
            else
                return {ret, mods};
        }
    }
    return { key_COUNT, k & kmod_mask };
}

void app::clear_non_global_keys() { clear_keys(key_MIN, key_GLOBAL); }
void app::clear_non_repeated_keys() { clear_keys(key_NO_REPEAT, key_COUNT); }

void app::on_key_up_down(const key_event& event, bool is_down, const sdl2::EvKey& ev) noexcept
{
    auto [x, mods] = resolve_keybinding(event.key, event.mods);
    static_assert(key_GLOBAL >= key_NO_REPEAT);

    if ((x == key_COUNT || x < key_GLOBAL) && do_imgui_key(ev, is_down) ||
        (x == key_COUNT || x == key_escape) && do_tests_key(event, is_down))
        clear_non_global_keys();
    else if (x >= key_NO_REPEAT)
        is_down && !event.is_repeated ? do_key(x, mods, event.key & ~SDLK_SCANCODE_MASK) : void();
    else
    {
        (*keys_)[x] = is_down;
        key_modifiers.data[size_t(x)] = mods;
    }
}

void app::on_text_input_event(const text_input_event& event) noexcept
{
    struct {
        accessor(Containers::StringView, text)
    } e = {event.text};
    if (_imgui->handleTextInputEvent(e))
        clear_non_global_keys();
}

void app::on_viewport_event(const Math::Vector2<int>& size) noexcept
{
    init_imgui(size);
}

void app::on_focus_out() noexcept
{
    update_cursor_tile(NullOpt);
    clear_keys();
}

void app::on_mouse_leave() noexcept
{
    update_cursor_tile(NullOpt);
}

void app::do_key(key k)
{
    do_key(k, get_key_modifiers(), 0);
}

int app::get_key_modifiers()
{
    return fixup_mods(M->get_mods());
}

void app::set_cursor_from_imgui()
{
    _imgui->updateApplicationCursor(M->application());
}

} // namespace floormat
