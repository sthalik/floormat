#include <cstddef>
#include "app.hpp"
#include "compat/fpu.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/ImGuiIntegration/Context.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <SDL_events.h>
#include <SDL_video.h>

namespace floormat {

app::app(const Arguments& arguments):
      Platform::Application{
          arguments,
          Configuration{}
              .setTitle("Test")
              .setSize({1024, 768}, dpi_policy::Physical)
              .setWindowFlags(Configuration::WindowFlag::Resizable),
          GLConfiguration{}
              .setSampleCount(4)
              .setFlags(GLConfiguration::Flag::GpuValidation)
      }
{
    if (!setSwapInterval(-1))
        (void)setSwapInterval(1);
    set_fp_mask();
    reset_camera_offset();
    update_window_scale(windowSize());
    setMinimalLoopPeriod(5);
    _imgui = ImGuiIntegration::Context(Vector2{windowSize()}, windowSize(), framebufferSize());
    SDL_MaximizeWindow(window());
    {
        auto c = _world[chunk_coords{0, 0}];
        make_test_chunk(*c);
    }
    timeline.start();
}

void app::viewportEvent(Platform::Sdl2Application::ViewportEvent& event)
{
    update_window_scale(event.windowSize());
    GL::defaultFramebuffer.setViewport({{}, event.windowSize()});
    _imgui.relayout(Vector2{event.windowSize()}, event.windowSize(), event.framebufferSize());
}


void app::mousePressEvent(Platform::Sdl2Application::MouseEvent& event)
{
    if (_imgui.handleMousePressEvent(event))
        return event.setAccepted();
    {
        if (_cursor_tile)
        {
            const auto& tile = *_cursor_tile;
            int button;
            switch (event.button())
            {
            case MouseEvent::Button::Left:   button = 0; break;
            case MouseEvent::Button::Right:  button = 1; break;
            case MouseEvent::Button::Middle: button = 2; break;
            case MouseEvent::Button::X1:     button = 5; break;
            case MouseEvent::Button::X2:     button = 6; break;
            default: button = -1; break;
            }
            do_mouse_click(tile, button);
        }
    }
}

void app::mouseReleaseEvent(Platform::Sdl2Application::MouseEvent& event)
{
    if (_imgui.handleMouseReleaseEvent(event))
        return event.setAccepted();
#if 0
    using Button = Platform::Sdl2Application::MouseEvent::Button;
    if (event.button() == Button::Left)
    {
    }
#endif
}

void app::mouseMoveEvent(Platform::Sdl2Application::MouseMoveEvent& event)
{
    _cursor_in_imgui = _imgui.handleMouseMoveEvent(event);
    _cursor_pixel = event.position();
    recalc_cursor_tile();
}

void app::mouseScrollEvent(Platform::Sdl2Application::MouseScrollEvent& event)
{
    if (_imgui.handleMouseScrollEvent(event))
        return event.setAccepted();
}

void app::textInputEvent(Platform::Sdl2Application::TextInputEvent& event)
{
    if (_imgui.handleTextInputEvent(event))
        return keys = {}, event.setAccepted();
}

void app::keyPressEvent(Platform::Sdl2Application::KeyEvent& event)
{
    if (_imgui.handleKeyPressEvent(event))
        return event.setAccepted();
    do_key(event.key(), event.modifiers(), true, event.isRepeated());
}

void app::keyReleaseEvent(Platform::Sdl2Application::KeyEvent& event)
{
    if (_imgui.handleKeyReleaseEvent(event))
        return keys = {}, event.setAccepted();
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
    _cursor_tile = std::nullopt;
}

void app::event_focus_in()
{
}

void app::event_mouse_leave()
{
    _cursor_pixel = std::nullopt;
    _cursor_tile = std::nullopt;
}

void app::event_mouse_enter()
{
}

} // namespace floormat

MAGNUM_APPLICATION_MAIN(floormat::app)

#ifdef _MSC_VER
#include <cstdlib> // for __arg{c,v}
#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wmain"
#endif
extern "C" int __stdcall WinMain(void*, void*, void*, int);

extern "C" int __stdcall WinMain(void*, void*, void*, int)
{
    return main(__argc, __argv);
}
#ifdef __clang__
#    pragma clang diagnostic pop
#endif
#endif
