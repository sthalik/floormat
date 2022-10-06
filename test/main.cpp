#include "app.hpp"
#include "loader.hpp"
#include <filesystem>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Magnum.h>

namespace Magnum::Examples {

app::app(const Arguments& arguments):
      Platform::WindowlessWglApplication{
          arguments,
          Configuration{}
      }
{
}

app::~app()
{
    loader_::destroy();
}

int app::exec()
{
    bool ret = true;
    ret &= test_json();
    return ret;
}

} // namespace Magnum::Examples

int main(int argc, char** argv)
{
    Magnum::Examples::app application{{argc, argv}};
    return application.exec();
}

