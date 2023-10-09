#include "app.hpp"
#include "bench.hpp"
#include "src/path-search.hpp"
#include "loader/loader.hpp"
#include <Magnum/Math/Functions.h>

namespace floormat {

void test_app::test_dijkstra()
{
    auto w = world();
    auto a = astar();

    constexpr auto wcx = 1, wcy = 1, wtx = 8, wty = 8, wox = 3, woy = 3;
    constexpr auto max_dist = (uint32_t)(Vector2i(Math::abs(wcx)+1, Math::abs(wcy)+1)*TILE_MAX_DIM*iTILE_SIZE2).length();
    constexpr auto wch = chunk_coords_{chunk_coords_{wcx, wcy, 0}};
    constexpr auto wt = local_coords{wtx, wty};
    constexpr auto wpos = global_coords{wch, wt};

    auto& ch  = w[chunk_coords_{0,0,0}];
#if 1
    auto& ch2 = w[wch];
    auto metal2 = tile_image_proto{loader.tile_atlas("metal2", {2, 2}, pass_mode::blocked), 0};

    ch[{4, 4}].wall_west()  = metal2;
    ch[{4, 4}].wall_north() = metal2;

    ch2[{ wtx,   wty   }].wall_west()  = metal2;
    ch2[{ wtx,   wty   }].wall_north() = metal2;
    ch2[{ wtx+1, wty   }].wall_west()  = metal2;
    ch2[{ wtx,   wty -1}].wall_north() = metal2;
#endif

    fm_assert(ch.is_passability_modified());

    auto do_bench = [&](int debug) {
      a.Dijkstra(w,
                 {{0,0,0}, {11,9}},    // from
                 {wpos, {wox, woy}},   // to
                 0, max_dist, {32,32}, // size
                 debug);
    };

    do_bench(0);
    bench_run("Dijkstra", [&] {
      a.Dijkstra(w,
                 {{0,0,0}, {11,9}},    // from
                 {wpos, {wox, woy}},   // to
                 0, max_dist, {32,32}, // size
                 1);
    });
}

} // namespace floormat
