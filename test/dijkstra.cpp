#include "app.hpp"
#include "src/path-search.hpp"
#include "loader/loader.hpp"
#include "loader/wall-cell.hpp"
#include <Magnum/Math/Functions.h>

namespace floormat {

void test_app::test_dijkstra()
{
    [[maybe_unused]] constexpr bool debug = false;

    auto A = astar{};
    auto w = world();

    constexpr auto wcx = 1, wcy = 1, wtx = 8, wty = 8, wox = 0, woy = 0;
    constexpr auto max_dist = (uint32_t)(Vector2i(Math::abs(wcx)+1, Math::abs(wcy)+1)*TILE_MAX_DIM*iTILE_SIZE2).length();
    constexpr auto wch = chunk_coords_{wcx, wcy, 0};
    constexpr auto wt = local_coords{wtx, wty};
    constexpr auto wpos = global_coords{wch, wt};

    auto& ch = w[wch];
    auto wall = wall_image_proto{loader.make_invalid_wall_atlas().atlas, 0};

    for (int16_t j = wcy - 1; j <= wcy + 1; j++)
        for (int16_t i = wcx - 1; i <= wcx + 1; i++)
        {
            auto &c = w[chunk_coords_{i, j, 0}];
            for (int k : {  3, 4, 5, 6, 11, 12, 13, 14, 15, })
            {
                c[{ k, k }].wall_north() = wall;
                c[{ k, k }].wall_west() = wall;
            }
        }

    ch[{ wtx,   wty   }].wall_west()  = wall;
    ch[{ wtx,   wty   }].wall_north() = wall;
    ch[{ wtx+1, wty   }].wall_west()  = wall;
    ch[{ wtx,   wty +1}].wall_north() = wall;

    for (int16_t j = wcy - 1; j <= wcy + 1; j++)
        for (int16_t i = wcx - 1; i <= wcx + 1; i++)
        {
            auto& c = w[chunk_coords_{i, j, 0}];
            c.mark_passability_modified();
            c.ensure_passability();
        }

    const auto run = [&](int debug) {
        return A.Dijkstra(w,
                          {{0,0,0}, {11,9}},    // from
                          {wpos, {wox, woy}},   // to
                          0, max_dist, {16,16}, // size
                          debug ? 1 : 0);
    };

    {
        constexpr auto min = (uint32_t)(TILE_SIZE2.length()*.25f) - uint32_t{1},
                       max = (uint32_t)(TILE_SIZE2*2.f).length() + uint32_t{1};
        auto result = run(debug);

        fm_assert(!result.is_found());
        fm_assert(!result.path().isEmpty());
        fm_assert(result.size() > 4);
        fm_assert(result.cost() > 1000);
        fm_assert(result.cost() < 3000);
        fm_assert(result.distance() > min);
        fm_assert(result.distance() < max);
    }

    {
        ch[{ wtx,   wty +1}].wall_north() = {};
        ch.mark_passability_modified();
        ch.ensure_passability();
        auto result = run(debug);

        fm_assert(result.is_found());
        fm_assert(!result.path().isEmpty());
        fm_assert(result.size() > 4);
        fm_assert(result.cost() > 1000);
        fm_assert(result.cost() < 3000);
        fm_assert(result.distance() == 0);
    }
}

} // namespace floormat
