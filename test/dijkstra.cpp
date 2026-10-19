#include "app.hpp"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/search-cache.hpp"
#include "src/search.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-pool-registry.hpp"
#include "src/point.hpp"
#include "src/intra-coord.inl"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/scenery-proto.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "loader/wall-cell.hpp"
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

constexpr auto flush_ch = chunk_coords_{0, 0, 0};
constexpr auto flush_bbox = Vector2ui{32};
constexpr auto flush_tile = local_coords{8, 8};

// Floor every chunk a search from `from` can test: its nodes lie within max_dist of `from`,
// and the cells tested around them one step further.
void add_floors(world& w, point from, uint32_t max_dist)
{
    const auto floor = tile_image_proto{ loader.ground_atlas("tiles"), 0 };
    const auto reach = Vector2i{(int)max_dist + Search::div_size.x()};
    const auto lo = (from - reach).chunk(), hi = (from + reach).chunk();
    for (auto j = lo.y; j <= hi.y; j++)
        for (auto i = lo.x; i <= hi.x; i++)
        {
            auto& c = w[{i, j, from.chunk3().z}];
            for (auto y = 0u; y < TILE_MAX_DIM; y++)
                for (auto x = 0u; x < TILE_MAX_DIM; x++)
                    c[{x, y}].ground() = floor;
            c.mark_passability_modified();
            c.ensure_passability();
        }
}

bool flush_box_free(world& w, point pt)
{
    const auto R = Range2D::fromCenter(Vector2(intra_coord{pt}.center_shifted()), Vector2(flush_bbox/2u));
    return Search::is_passable(w, flush_ch, R, Search::without_critters());
}

void add_block(world& w, local_coords tile, Vector2ub bbox_size, Vector2b bbox_offset = {})
{
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_size   = bbox_size;
    p.bbox_offset = bbox_offset;
    p.pass        = pass_mode::blocked;
    w.make_scenery(w.make_id(), {flush_ch, tile}, move(p));
}

// The goal's bbox touches a collider. Whether the touching coordinate falls on a pass-grid cell
// boundary must not decide whether it's found.
template<typename F>
void check_flush_goal(const char* name, F&& setup, Vector2b goal_offset, Vector2i toward, bool reachable = true)
{
    constexpr uint32_t max_dist = 1024;
    const auto from = point{global_coords{flush_ch, {8, 12}}, {}};
    auto w = world();
    add_floors(w, from, max_dist);
    setup(w);
    auto& c = w[flush_ch];
    c.mark_passability_modified();
    c.ensure_passability();

    const auto goal = point{global_coords{flush_ch, flush_tile}, goal_offset};
    fm_assert(flush_box_free(w, goal));
    fm_assert(!flush_box_free(w, point::normalize_coords(goal, toward)));

    const auto& pred = Search::without_critters();
    auto A = astar{};
    auto& pool16 = A.pool_registry().pool_for(flush_bbox.x());
    auto pool8 = Grid::Pass::Pool{Grid::Pass::Params{8, flush_bbox.x(), pass_mode::pass}, pred};

    for (auto* pool : { &pool16, &pool8 })
    {
        const auto div = pool->params().div_size();
        auto r1 = A.Dijkstra<0, true>(w, *pool, from, goal, max_dist, flush_bbox, pred);
        auto r2 = A.Dijkstra<0, false>(w, *pool, from, goal, max_dist, flush_bbox, pred);
        if (r1.is_found() != reachable || r2.is_found() != reachable)
            fm_abort("flush goal '%s' at div %u: found %d, without closest %d",
                     name, div, (int)r1.is_found(), (int)r2.is_found());
        if (reachable)
        {
            fm_assert(r1.path().back() == goal);
            fm_assert(r2.cost() == r1.cost());
        }
        else
        {
            fm_assert(!r1.path().isEmpty());
            fm_assert(r2.path().isEmpty());
        }
    }
}

void test_flush_goals()
{
    const auto wall = wall_image_proto{loader.invalid_wall_atlas().atlas, 0};
    const auto depth = (int)wall.atlas->info().depth;
    constexpr auto h = (int)flush_bbox.x()/2;
    const auto far = (int8_t)(half_tile<int> - depth - h);

    check_flush_goal("wall north", [&](world& w) { w[flush_ch][flush_tile].wall_north() = wall; },
                     {0, (int8_t)(h - half_tile<int>)}, {0, -1});
    check_flush_goal("wall west", [&](world& w) { w[flush_ch][flush_tile].wall_west() = wall; },
                     {(int8_t)(h - half_tile<int>), 0}, {-1, 0});
    check_flush_goal("wall south", [&](world& w) { w[flush_ch][{8, 9}].wall_north() = wall; },
                     {0, far}, {0, 1});
    check_flush_goal("wall east", [&](world& w) { w[flush_ch][{9, 8}].wall_west() = wall; },
                     {far, 0}, {1, 0});

    // bbox sizes must be even, so the odd offset is what puts the south edge at an odd coordinate
    constexpr auto size = Vector2ub{tile_size_xy, 60};
    constexpr auto odd = Vector2b{0, 1};
    check_flush_goal("odd scenery north", [&](world& w) { add_block(w, {8, 7}, size, odd); },
                     {0, (int8_t)(odd.y() + size.y()/2 + h - tile_size_xy)}, {0, -1});

    // A gap as wide as the bbox and two tiles long. No cell in it has its bit set, and the free
    // nodes past its ends are too far from the middle to hop to it.
    constexpr auto gap_side = Vector2ub{(uint8_t)(2*tile_size_xy - flush_bbox.x()), 2*tile_size_xy};
    check_flush_goal("box-wide gap", [&](world& w) {
        add_block(w, {7, 8}, gap_side);
        add_block(w, {9, 8}, gap_side);
    }, {}, {1, 0}, false);
}

} // namespace

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
    const auto from = point{{0, 0, 0}, {11, 9}};

    add_floors(w, from, max_dist);
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
    auto& pool = A.pool_registry().pool_for(own_size.x());

    const auto run = [&]<bool debug, bool closest = true>(point to) {
            return A.Dijkstra<debug ? 1 : 0, closest>(
                w,
                from,
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
        fm_assert(result.is_found());
        fm_assert(result.path().back() == blocked_cell_goal);

        auto result2 = run.operator()<debug, false>(blocked_cell_goal);
        fm_assert(result2.is_found());
        fm_assert(result2.cost() == result.cost());
    }

    test_flush_goals();
}

} // namespace floormat
