#include "app.hpp"
#include "path-search.hpp"
#include <chrono>
#include <Corrade/Containers/StringView.h>

namespace floormat {

namespace {

template<typename F>
requires requires (F& fun) { fun(); }
void bench_run(StringView name, F&& fun)
{
    using namespace std::chrono;
    using clock = high_resolution_clock;
    const auto t0 = clock::now();
    fun();
    for (int i = 0; i < 1000; i++)
        fun();
    const auto tm = clock::now() - t0;
    Debug{} << "test" << name << "took" << duration_cast<milliseconds>(tm).count() << "ms.";
}

} // namespace

void test_app::test_dijkstra()
{
    auto w = world();
    auto a = astar();

    bench_run("Dijkstra", [&] {
      a.Dijkstra(w, {}, 0, {{0, 0, 0}, {}}, {{1, 1, 0}, {}},
                 1*TILE_MAX_DIM*iTILE_SIZE2.x());
    });
}

} // namespace floormat
