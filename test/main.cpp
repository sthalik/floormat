#include "app.hpp"
#include "loader/loader.hpp"

namespace floormat {

test_app::test_app(const Arguments& arguments):
      Application {
          arguments,
          Configuration{}
      }
{
}

test_app::~test_app()
{
    loader_::destroy();
}

int test_app::exec()
{
    bool ret = true;
    ret &= test_json();
    ret &= test_tile_iter();
    ret &= test_const_math();
    ret &= test_serializer();
    return !ret;
}

} // namespace floormat

int main(int argc, char** argv)
{
    floormat::test_app application{{argc, argv}};
    return application.exec();
}
