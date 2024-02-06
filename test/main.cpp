#include "app.hpp"
#include "compat/assert.hpp"
#include "loader/loader.hpp"
#include <stdlib.h>
#include <cstdlib>

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
    loader.destroy();
}

int test_app::exec()
{
    test_coords();
    test_json();
    test_tile_iter();
    test_magnum_math();
    test_entity();
    test_math();
    test_hash();
    test_loader();
    test_bitmask();
    test_wall_atlas();
    test_wall_atlas2();
    test_serializer_1();
    test_serializer_2();
    test_scenery();
    test_raycast();
    test_path_search_node_pool();
    test_path_search();
    test_dijkstra();

    zzz_test_misc();

    return 0;
}

} // namespace floormat

int main(int argc, char** argv)
{
#ifdef _WIN32
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    if (const auto* s = std::getenv("MAGNUM_LOG"); !s || !*s)
        _putenv("MAGNUM_LOG=quiet");
#else
        setenv("MAGNUM_LOG", "quiet", 0);
#endif
    floormat::test_app application{{argc, argv}};
    return application.exec();
}
