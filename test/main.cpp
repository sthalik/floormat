#include "app.hpp"
#include "compat/assert.hpp"
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
    test_json();
    test_tile_iter();
    test_const_math();
    test_serializer();
    test_entity();
    test_quadtree();
    return 0;
}

} // namespace floormat

int main(int argc, char** argv)
{
    floormat::test_app application{{argc, argv}};
    return application.exec();
}
