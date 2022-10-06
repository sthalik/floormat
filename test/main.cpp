#include "app.hpp"
#include "loader.hpp"
#include <filesystem>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Magnum.h>

namespace Magnum::Examples {

app::app(const Arguments& arguments):
      Platform::Application{
          arguments,
          Configuration{}
              .setTitle("Test")
              .setSize({1024, 768}, Platform::Implementation::Sdl2DpiScalingPolicy::Physical),
          GLConfiguration{}
              .setSampleCount(4)
              .setFlags(Platform::Sdl2Application::GLConfiguration::Flag::Debug)
      }
{
    if (auto path_opt = Utility::Path::executableLocation(); path_opt)
        std::filesystem::current_path(std::string{Utility::Path::split(*path_opt).first()});
}

app::~app()
{
    loader_::destroy();
}

void app::drawEvent()
{
    test();
    Platform::Sdl2Application::exit(0);
}

void app::test()
{
    test_json();
}

} // namespace Magnum::Examples

using namespace Magnum::Examples;

MAGNUM_APPLICATION_MAIN(Magnum::Examples::app)
