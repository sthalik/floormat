#include "main-impl.hpp"
#include "floormat/app.hpp"
#include "floormat/events.hpp"
#include <SDL_events.h>
//#include <SDL_video.h>

namespace floormat {

void main_impl::viewportEvent(Platform::Sdl2Application::ViewportEvent& event)
{
    fm_assert(event.framebufferSize() == event.windowSize());
    recalc_viewport(event.windowSize());
    app.on_viewport_event(event.windowSize());
}

void main_impl::mousePressEvent(Platform::Sdl2Application::MouseEvent& event)
{
    app.on_mouse_up_down({event.position(),
                          (SDL_Keymod)(std::uint16_t)event.modifiers(),
                          mouse_button(SDL_BUTTON((std::uint8_t)event.button())),
                          std::uint8_t(std::min(255, event.clickCount()))},
                         true);
}

void main_impl::mouseReleaseEvent(Platform::Sdl2Application::MouseEvent& event)
{
    app.on_mouse_up_down({event.position(),
                          (SDL_Keymod)(std::uint16_t)event.modifiers(),
                          mouse_button(SDL_BUTTON((std::uint8_t)event.button())),
                          std::uint8_t(std::min(255, event.clickCount()))},
                         false);
}

void main_impl::mouseMoveEvent(Platform::Sdl2Application::MouseMoveEvent& event)
{
    app.on_mouse_move({event.position(), event.relativePosition(),
                       (mouse_button)(std::uint8_t)(std::uint32_t)event.buttons(),
                       (SDL_Keymod)(std::uint16_t)event.modifiers()});
}

void main_impl::mouseScrollEvent(Platform::Sdl2Application::MouseScrollEvent& event)
{
    app.on_mouse_scroll({event.offset(), event.position(),
                         (SDL_Keymod)(std::uint16_t)event.modifiers()});
}

void main_impl::textInputEvent(Platform::Sdl2Application::TextInputEvent& event)
{
    app.on_text_input_event({event.text()});
}

#if 0
void main_impl::textEditingEvent(Platform::Sdl2Application::TextEditingEvent& event)
{
    app.on_text_editing_event({event.text(), event.start(), event.length()})
}
#endif

void main_impl::keyPressEvent(Platform::Sdl2Application::KeyEvent& event)
{
    app.on_key_up_down({(SDL_Keycode)(std::uint32_t)event.key(),
                        (SDL_Keymod)(std::uint16_t)event.modifiers(),
                        event.isRepeated()},
                       true);
}

void main_impl::keyReleaseEvent(Platform::Sdl2Application::KeyEvent& event)
{
    app.on_key_up_down({(SDL_Keycode)(std::uint32_t)event.key(),
                        (SDL_Keymod)(std::uint16_t)event.modifiers(),
                        event.isRepeated()},
                       false);
}

static any_event make_any_event(const SDL_Event& e)
{
    static_assert(sizeof(SDL_Event) <= sizeof(any_event::buf));
    any_event ret;
    std::memcpy(&ret.buf, &e, sizeof(SDL_Event));
    return ret;
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
            return app.on_focus_in();
        case SDL_WINDOWEVENT_LEAVE:
            return app.on_mouse_leave();
        case SDL_WINDOWEVENT_ENTER:
            return app.on_mouse_enter();
        default: {
            return app.on_any_event(make_any_event(event));
        }
        }
    }
    else
        return app.on_any_event(make_any_event(event));
}

} // namespace floormat

