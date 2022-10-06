#include <Magnum/Magnum.h>
#include <Magnum/Platform/Sdl2Application.h>
#include "compat/assert.hpp"
#include "tile-atlas.hpp"
#include "serialize/tile-atlas.hpp"
#include "serialize/json-helper.hpp"
#include "loader.hpp"
#include "serialize/magnum-vector.hpp"

namespace Magnum::Examples {

struct app final : Platform::Application
{
    using dpi_policy = Platform::Implementation::Sdl2DpiScalingPolicy;

    explicit app(const Arguments& arguments);
    ~app();
    void drawEvent() override;
    void test();
};

app::app(const Arguments& arguments):
      Platform::Application{
          arguments,
          Configuration{}
              .setTitle("Test")
              .setSize({1024, 768}, dpi_policy::Physical),
          GLConfiguration{}
              .setSampleCount(4)
              //.setFlags(Platform::Sdl2Application::GLConfiguration::Flag::Debug)
      }
{
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

void app::test() // NOLINT(readability-convert-member-functions-to-static)
{
    auto atlas = loader.tile_atlas("../share/game/images/metal1.tga", {2, 2});
    bool ret = json_helper<std::shared_ptr<tile_atlas>>::to_json(atlas, "f:/dev/game/build/test/atlas.json");
    ASSERT(ret);
}

} // namespace Magnum::Examples

using namespace Magnum::Examples;

MAGNUM_APPLICATION_MAIN(Magnum::Examples::app)

#ifdef _MSC_VER
#   include <cstdlib>
#   ifdef __clang__
#       pragma clang diagnostic ignored "-Wmissing-prototypes"
#       pragma clang diagnostic ignored "-Wmain"
#   endif

extern "C" int __stdcall WinMain(void*, void*, void*, int /* nCmdShow */) {
    return main(__argc, __argv);
}
#endif
