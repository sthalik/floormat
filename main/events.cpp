#include "main-impl.hpp"
#include "floormat/app.hpp"
#include "floormat/events.hpp"
#include "sdl-fwd.inl"
#include <cstring>
#include <SDL_events.h>
#include <SDL_keyboard.h>

namespace floormat {

namespace {

using Buttons = Platform::Sdl2Application::Pointers;

mouse_button pointer_to_button_mask(Buttons b) { return mouse_button((uint8_t)b); }

any_event make_any_event(const SDL_Event& e)
{
    static_assert(sizeof(SDL_Event) <= sizeof(any_event::buf));
    any_event ret;
    std::memcpy(&ret.buf, &e, sizeof(SDL_Event));
    return ret;
}

} // namespace

void main_impl::viewportEvent(ViewportEvent& event)
{
    _framebuffer_size = event.framebufferSize();
    recalc_viewport(event.framebufferSize(), event.windowSize());
    app.on_viewport_event(event.framebufferSize());
}

void main_impl::pointerPressEvent(PointerEvent& ev)
{
    app.on_mouse_up_down({
        ev.position() * _virtual_scale,
        (SDL_Keymod)(uint16_t)ev.modifiers(),
        pointer_to_button_mask(ev.pointer()),
        uint8_t(std::min(255, ev.clickCount())),
    }, true, {ev});
}

void main_impl::pointerReleaseEvent(PointerEvent& ev)
{
    app.on_mouse_up_down({
        ev.position() * _virtual_scale,
        (SDL_Keymod)(uint16_t)ev.modifiers(),
        pointer_to_button_mask(ev.pointer()),
        uint8_t(std::min(255, ev.clickCount())),
    }, false, {ev});
}

void main_impl::pointerMoveEvent(PointerMoveEvent& ev)
{
    app.on_mouse_move({
        ev.position() * _virtual_scale,
        (SDL_Keymod)(uint16_t)ev.modifiers(),
        pointer_to_button_mask(ev.pointers()),
        ev.isPrimary(),
    }, {ev});
}

void main_impl::scrollEvent(ScrollEvent& ev)
{
    app.on_mouse_scroll({
        ev.offset(), ev.position() * _virtual_scale,
        (SDL_Keymod)(uint16_t)ev.modifiers(),
    }, {ev});
}

void main_impl::textInputEvent(TextInputEvent& event)
{
    app.on_text_input_event({event.text()});
}

#if 0
void main_impl::textEditingEvent(TextEditingEvent& event)
{
    app.on_text_editing_event({event.text(), event.start(), event.length()})
}
#endif

void main_impl::keyPressEvent(KeyEvent& event)
{
    app.on_key_up_down({
        (SDL_Keycode)(uint32_t)event.key(),
        (SDL_Keymod)(uint16_t)event.modifiers(),
        event.isRepeated()
    }, true, {event});
}

void main_impl::keyReleaseEvent(KeyEvent& event)
{
    app.on_key_up_down({
        (SDL_Keycode)(uint32_t)event.key(),
        (SDL_Keymod)(uint16_t)event.modifiers(),
        event.isRepeated()
    }, false, {event});
}

void main_impl::anyEvent(SDL_Event& event)
{
    if (event.type == SDL_WINDOWEVENT)
    {
        update_window_state();
        switch (event.window.event)
        {
        case SDL_WINDOWEVENT_FOCUS_LOST:
            return app.on_focus_out();
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            _mouse_cursor = (uint32_t)-1;
            return app.on_focus_in();
        case SDL_WINDOWEVENT_LEAVE:
            return app.on_mouse_leave();
        case SDL_WINDOWEVENT_ENTER:
            _mouse_cursor = (uint32_t)-1;
            return app.on_mouse_enter();
        default:
            return app.on_any_event(make_any_event(event));
        }
    }
    else
        return app.on_any_event(make_any_event(event));
}

int floormat_main::get_mods() noexcept
{
    return SDL_GetModState();
}

} // namespace floormat
