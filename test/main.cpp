#include "app.hpp"
#include "loader/loader.hpp"

namespace floormat {

floormat::floormat(const Arguments& arguments):
      FM_APPLICATION {
          arguments,
          Configuration{}
      }
{
}

floormat::~floormat()
{
    loader_::destroy();
}

int floormat::exec()
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
    floormat::floormat application{{argc, argv}};
    return application.exec();
}
