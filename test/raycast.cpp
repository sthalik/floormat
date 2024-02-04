#include "app.hpp"
#include "src/raycast.hpp"
#include "src/world.hpp"

namespace floormat {

namespace {

#warning TODO!

void test1()
{
    // try reproducing the evil bug in src/raycast.cpp:285:
    //   c = w.at({last_ch + Vector2i{i - 1, j - 1}});
    // was incorrectly:
    //   c = w.at({last_ch - Vector2i{i - 1, j - 1}});
}

} // namespace

void test_app::test_raycast()
{

}

} // namespace floormat::test_app
