#include "app.hpp"

namespace Magnum::Examples {

app::app(const Arguments& arguments):
      Platform::Application{
          arguments,
          Configuration{}
              .setTitle("Test")
              .setSize({1024, 768}, dpi_policy::Physical),
          GLConfiguration{}
              .setSampleCount(4)
              .setFlags(GLConfiguration::Flag::GpuValidation)
      }
{
    reset_camera_offset();
    timeline.start();
}

void app::update_window_scale()
{
    auto sz = windowSize();
    _shader.set_scale({ (float)sz[0], (float)sz[1] });
}

void app::update(float dt)
{
    do_camera(dt);
    if (keys[key::quit])
        Platform::Sdl2Application::exit(0);
}

} // namespace Magnum::Examples
