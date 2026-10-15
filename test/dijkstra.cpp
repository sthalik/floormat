#include "app.hpp"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/search-cache.hpp"
#include "src/search.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-pass-pool.hpp"
#include "src/point.hpp"
#include "src/intra-coord.inl"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "loader/wall-cell.hpp"
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

void Test::test_dijkstra()
{
    [[maybe_unused]] constexpr bool debug = false;

    auto A = astar{};
    auto w = world();

    constexpr auto wcx = 1, wcy = 1, wtx = 8, wty = 8, wox = 0, woy = 0;
    constexpr auto max_dist = (uint32_t)(Vector2i(Math::abs(wcx)+1, Math::abs(wcy)+1)*chunk_size<Vector2i>).length();
    constexpr auto wch = chunk_coords_{wcx, wcy, 0};
    constexpr auto wt = local_coords{wtx, wty};
    constexpr auto wpos = global_coords{wch, wt};

    auto& ch = w[wch];
    auto wall = wall_image_proto{loader.invalid_wall_atlas().atlas, 0};

    for (int16_t j = wcy - 1; j <= wcy + 1; j++)
        for (int16_t i = wcx - 1; i <= wcx + 1; i++)
        {
            auto &c = w[{i, j, 0}];
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
            auto& c = w[{i, j, 0}];
            c.mark_passability_modified();
            c.ensure_passability();
        }

    constexpr auto own_size = Vector2ui{16};
    const auto goal = point{wpos, {wox, woy}};
    const auto& pred = Search::without_critters();
    auto& pool = A.pass_pool_registry().pool_for(own_size.x());

    const auto run = [&]<bool debug, bool closest = true>(point to) {
            return A.Dijkstra<debug ? 1 : 0, closest>(
                w,
                {{0, 0, 0}, {11, 9}}, // from
                to,
                max_dist, own_size,
                pred);
    };

    {
        constexpr auto min = (uint32_t)(TILE_SIZE2.length()*.25f) - uint32_t{1},
                       max = (uint32_t)(TILE_SIZE2*2.f).length() + uint32_t{1};
        auto result = run.operator()<debug>(goal);

        fm_assert(!result.is_found());
        fm_assert(!result.path().isEmpty());
        fm_assert(result.size() > 2);
        fm_assert(result.cost() > 1000);
        fm_assert(result.cost() < 3000);
        fm_assert(result.distance() > min);
        fm_assert(result.distance() < max);

        fm_assert(A.cache()->is_passable_for_bbox(w, pool, goal, pred));
        auto result2 = run.operator()<debug, false>(goal);
        fm_assert(!result2.is_found());
        fm_assert(result2.path().isEmpty());
    }

    {
        ch[{ wtx,   wty +1}].wall_north() = {};
        ch.mark_passability_modified();
        ch.ensure_passability();
        auto result = run.operator()<debug>(goal);

        fm_assert(result.is_found());
        fm_assert(!result.path().isEmpty());
        fm_assert(result.size() > 2);
        fm_assert(result.cost() > 1000);
        fm_assert(result.cost() < 3000);
        fm_assert(result.distance() == 0);

        auto result2 = run.operator()<debug, false>(goal);
        fm_assert(result2.is_found());
        fm_assert(result2.cost() == result.cost());
        fm_assert(result2.size() == result.size());
    }

    {
        const auto blocked_cell_goal = [&] {
            for (int y = -half_tile<int>; y < half_tile<int>; y++)
                for (int x = -half_tile<int>; x < half_tile<int>; x++)
                {
                    const auto pt = point{wpos, {(int8_t)x, (int8_t)y}};
                    const auto R = Range2D::fromCenter(Vector2(intra_coord{pt}.center_shifted()), Vector2(own_size/2u));
                    if (!A.cache()->is_passable_for_bbox(w, pool, pt, pred) && Search::is_passable(w, wch, R, pred))
                        return pt;
                }
            fm_abort("no goal with a blocked cell");
        }();

        auto result = run.operator()<debug>(blocked_cell_goal);
        fm_assert(!result.is_found());
        fm_assert(!result.path().isEmpty());

        auto result2 = run.operator()<debug, false>(blocked_cell_goal);
        fm_assert(!result2.is_found());
        fm_assert(result2.path().isEmpty());
    }
}

} // namespace floormat
