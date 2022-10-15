#include <cstddef>
#include "app.hpp"
#include "compat/fpu.hpp"
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/ImGuiIntegration/Context.h>
#include <Magnum/ImGuiIntegration/Context.hpp>

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
    setup_menu();
    SDL_MaximizeWindow(window());
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
}

void app::mouseReleaseEvent(Platform::Sdl2Application::MouseEvent& event)
{
    if (_imgui.handleMouseReleaseEvent(event))
        return event.setAccepted();
}

void app::mouseMoveEvent(Platform::Sdl2Application::MouseMoveEvent& event)
{
    if (_imgui.handleMouseMoveEvent(event))
        return event.setAccepted();
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

void app::update(float dt)
{
    do_camera(dt);
    if (keys[key::quit])
        Platform::Sdl2Application::exit(0);
}

} // namespace floormat
