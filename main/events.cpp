#pragma once
#include "app.hpp"
#include <Magnum/ImGuiIntegration/Context.hpp>

#include <cstdio>
#include <SDL_events.h>
#include <SDL_video.h>

namespace floormat {

void app::viewportEvent(Platform::Sdl2Application::ViewportEvent& event)
{
    fm_assert(event.framebufferSize() == event.windowSize());
    recalc_viewport(event.windowSize());
}

void app::mousePressEvent(Platform::Sdl2Application::MouseEvent& event)
{
    if (_imgui.handleMousePressEvent(event))
        return event.setAccepted();
    else if (_cursor_tile)
    {
        const auto& tile = *_cursor_tile;
        do_mouse_click(tile, (int)event.button());
    }
}

void app::mouseReleaseEvent(Platform::Sdl2Application::MouseEvent& event)
{
    if (_imgui.handleMouseReleaseEvent(event))
        return event.setAccepted();
    do_mouse_release((int)event.button());
}

void app::mouseMoveEvent(Platform::Sdl2Application::MouseMoveEvent& event)
{
    _cursor_in_imgui = _imgui.handleMouseMoveEvent(event);
    if (_cursor_in_imgui)
        _cursor_pixel = std::nullopt;
    else
        _cursor_pixel = event.position();
    recalc_cursor_tile();
    if (_cursor_tile)
        do_mouse_move(*_cursor_tile);
}

void app::mouseScrollEvent(Platform::Sdl2Application::MouseScrollEvent& event)
{
    if (_imgui.handleMouseScrollEvent(event))
        return event.setAccepted();
}

void app::textInputEvent(Platform::Sdl2Application::TextInputEvent& event)
{
    if (_imgui.handleTextInputEvent(event))
    {
        keys = {};
        event.setAccepted();
    }
}

void app::keyPressEvent(Platform::Sdl2Application::KeyEvent& event)
{
    if (_imgui.handleKeyPressEvent(event))
    {
        keys = {};
        return event.setAccepted();
    }
    do_key(event.key(), event.modifiers(), true, event.isRepeated());
}

void app::keyReleaseEvent(Platform::Sdl2Application::KeyEvent& event)
{
    if (_imgui.handleKeyReleaseEvent(event))
    {
        keys = {};
        return event.setAccepted();
    }
    do_key(event.key(), event.modifiers(), false, false);
}

void app::anyEvent(SDL_Event& event)
{
    if (event.type == SDL_WINDOWEVENT)
        switch (event.window.event)
        {
        case SDL_WINDOWEVENT_FOCUS_LOST:
            return event_focus_out();
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            return event_focus_in();
        case SDL_WINDOWEVENT_LEAVE:
            return event_mouse_leave();
        case SDL_WINDOWEVENT_ENTER:
            return event_mouse_enter();
        default:
            std::fputs("", stdout); break; // put breakpoint here
        }
}

void app::event_focus_out()
{
    _cursor_pixel = std::nullopt;
    recalc_cursor_tile();
}

void app::event_focus_in()
{
}

void app::event_mouse_leave()
{
    _cursor_pixel = std::nullopt;
    recalc_cursor_tile();
}

void app::event_mouse_enter()
{
}


} // namespace floormat
