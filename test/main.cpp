#include "app.hpp"
#include "compat/assert.hpp"
#include "loader.hpp"
#include <filesystem>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StringStlView.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Magnum.h>

namespace floormat {

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
    ret &= test_tile_iter();
    ret &= test_const_math();
    return !ret;
}

} // namespace floormat

int main(int argc, char** argv)
{
    floormat::app application{{argc, argv}};
    return application.exec();
}

