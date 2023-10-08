#include "app.hpp"
#include "path-search.hpp"

namespace floormat {

void test_app::test_dijkstra()
{
    auto w = world();
    auto a = astar();

    a.Dijkstra(w, {}, 0, {{0, 0, 0}, {}}, {{1, 1, 0}, {}},
               1*TILE_MAX_DIM*iTILE_SIZE2.x());
}

} // namespace floormat
