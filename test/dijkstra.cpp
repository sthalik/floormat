#include "app.hpp"
#include "path-search.hpp"
#include <chrono>

namespace floormat {

void test_app::test_dijkstra()
{
    auto w = world();
    auto a = astar();

    using namespace std::chrono;
    using clock = high_resolution_clock;

    const auto t0 =  clock::now();

    for (int i = 0; i < 10; i++)
    {
        a.Dijkstra(w, {}, 0, {{0, 0, 0}, {}}, {{1, 1, 0}, {}},
                       1*TILE_MAX_DIM*iTILE_SIZE2.x());
    }
    const auto tm = clock::now() - t0;
    Debug{} << "test took" << std::chrono::duration_cast<milliseconds>(tm).count() << "ms.";
}

} // namespace floormat
