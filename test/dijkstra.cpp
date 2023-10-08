#include "app.hpp"
#include "path-search.hpp"
#include "loader/loader.hpp"
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
#if 0
    for (int i = 0; i < 20; i++)
        fun();
    const auto t0 = clock::now();
    for (int i = 0; i < 1000; i++)
        fun();
#else
    const auto t0 = clock::now();
    fun();
#endif
    const auto tm = clock::now() - t0;
    Debug{} << "test" << name << "took" << duration_cast<milliseconds>(tm).count() << "ms.";
}

} // namespace

void test_app::test_dijkstra()
{
    auto w = world();
    auto a = astar();

    auto metal2 = tile_image_proto{loader.tile_atlas("metal2", {2, 2}, pass_mode::blocked), 0};
    auto& ch = w[chunk_coords_{0,0,0}];

#if 0
    ch[{4, 4}].wall_west()  = metal2;
    ch[{4, 4}].wall_north() = metal2;
    ch[{8, 8}].wall_west()  = metal2;
    ch[{8, 8}].wall_north() = metal2;
    ch[{9, 8}].wall_west()  = metal2;
    ch[{8, 7}].wall_north() = metal2;
#endif
    ch.mark_modified();

    bench_run("Dijkstra", [&] {
      constexpr auto max_dist = Vector2ui(2*TILE_MAX_DIM*iTILE_SIZE2).length();
      a.Dijkstra(w,
                 {{0, 0, 0},   {0, 0}}, // from
                 {{16, 16, 0}, {7, 9}}, // to
                 0, max_dist, {32,32}); // size
    });
}

} // namespace floormat
