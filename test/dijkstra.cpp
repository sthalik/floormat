#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "src/anim-atlas.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/nanosecond.inl"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/search-cache.hpp"
#include "src/search.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-pool-registry.hpp"
#include "src/point.inl"
#include "src/intra-coord.inl"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/scenery-proto.hpp"
#include "src/tile-constants.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "loader/wall-cell.hpp"
#include <mg/Functions.h>
#include <mg/Range.h>

namespace floormat {

namespace {

constexpr auto flush_ch = chunk_coords_{0, 0, 0};
constexpr auto flush_tile = local_coords{8, 8};

// Floor every chunk a search from `from` can test: its nodes lie within max_dist of `from`,
// and the cells tested around them one step of `cell` px further.
void add_floors(world& w, point from, uint32_t max_dist, uint32_t cell)
{
    const auto floor = tile_image_proto{ loader.ground_atlas("tiles"), 0 };
    const auto reach = Vector2i{(int)(max_dist + cell)};
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

bool box_free(world& w, point pt, Vector2ui size)
{
    const auto min = Vector2(intra_coord{pt}.center_shifted() - Vector2i(size/2u));
    return Search::is_passable(w, pt.chunk3(), Range2D{min, min + Vector2(size)}, Search::without_critters());
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
void check_flush_goal(const char* name, uint32_t bbox, F&& setup, Vector2b goal_offset, Vector2i toward, bool reachable = true)
{
    constexpr uint32_t max_dist = 1024;
    const auto from = point{global_coords{flush_ch, {8, 12}}, {}};
    auto w = world();
    add_floors(w, from, max_dist, bbox);
    setup(w);
    auto& c = w[flush_ch];
    c.mark_passability_modified();
    c.ensure_passability();

    const auto goal = point{global_coords{flush_ch, flush_tile}, goal_offset};
    fm_assert(box_free(w, goal, Vector2ui(bbox)));
    fm_assert(!box_free(w, point::normalize_coords(goal, toward), Vector2ui(bbox)));

    const auto& pred = Search::without_critters();
    auto A = astar{};
    auto& pool16 = A.pool_registry().pool_for(bbox);
    auto pool8 = Grid::Pass::Pool{Grid::Pass::Params{8, bbox, pass_mode::pass}, pred};

    for (auto* pool : { &pool16, &pool8 })
    {
        const auto div = pool->params().div_size();
        auto r1 = A.Dijkstra<0, true>(w, *pool, from, goal, max_dist, Vector2ui(bbox), pred);
        auto r2 = A.Dijkstra<0, false>(w, *pool, from, goal, max_dist, Vector2ui(bbox), pred);
        if (r1.is_found() != reachable || r2.is_found() != reachable)
            fm_abort("flush goal '%s' bbox %u at div %u: found %d, without closest %d",
                     name, bbox, div, (int)r1.is_found(), (int)r2.is_found());
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

void test_flush_goals(uint32_t bbox)
{
    const auto wall = wall_image_proto{loader.invalid_wall_atlas().atlas, 0};
    const auto depth = (int)wall.atlas->info().depth;
    const auto lo = (int)bbox/2, hi = (int)bbox - lo;
    const auto far = (int8_t)(half_tile<int> - depth - hi);

    check_flush_goal("wall north", bbox, [&](world& w) { w[flush_ch][flush_tile].wall_north() = wall; },
                     {0, (int8_t)(lo - half_tile<int>)}, {0, -1});
    check_flush_goal("wall west", bbox, [&](world& w) { w[flush_ch][flush_tile].wall_west() = wall; },
                     {(int8_t)(lo - half_tile<int>), 0}, {-1, 0});
    check_flush_goal("wall south", bbox, [&](world& w) { w[flush_ch][{8, 9}].wall_north() = wall; },
                     {0, far}, {0, 1});
    check_flush_goal("wall east", bbox, [&](world& w) { w[flush_ch][{9, 8}].wall_west() = wall; },
                     {far, 0}, {1, 0});

    // Both put the block's south edge at an odd coordinate.
    for (const auto& [size, offset] : { std::pair{Vector2ub{tile_size_xy, 60}, Vector2b{0, 1}},
                                        std::pair{Vector2ub{tile_size_xy, 61}, Vector2b{0, 0}} })
    {
        const auto south = offset.y() + (size.y() - size.y()/2) - tile_size_xy;
        check_flush_goal("odd scenery north", bbox, [&](world& w) { add_block(w, {8, 7}, size, offset); },
                         {0, (int8_t)(south + lo)}, {0, -1});
    }

    // A gap as wide as the bbox. No cell in it has its bit set, and the free nodes past its ends
    // are too far from the middle to move straight to it. Two tiles long, the goal's seed search reaches
    // them. Four tiles long, it can't.
    const auto gap_side = Vector2ub{(uint8_t)(2*tile_size_xy - bbox), 2*tile_size_xy};
    check_flush_goal("box-wide gap", bbox, [&](world& w) {
        add_block(w, {7, 8}, gap_side);
        add_block(w, {9, 8}, gap_side);
    }, {}, {1, 0});
    check_flush_goal("long box-wide gap", bbox, [&](world& w) {
        add_block(w, {7, 7}, gap_side);
        add_block(w, {7, 9}, gap_side);
        add_block(w, {9, 7}, gap_side);
        add_block(w, {9, 9}, gap_side);
    }, {}, {1, 0}, false);
}

// The start touches a collider on its high x side, so the cell holding it is closed.
void check_closed_start_cell(uint32_t bbox, int8_t start_x)
{
    constexpr uint32_t max_dist = 1024;
    constexpr auto start_tile = local_coords{5, 5}, block_tile = local_coords{6, 5};
    constexpr auto block_size = Vector2ub{20, tile_size_xy};
    const auto from = point{global_coords{flush_ch, start_tile}, {start_x, 0}};
    const auto goal = point{global_coords{flush_ch, {1, 5}}, {}};
    const auto block_x = (int)block_tile.x * tile_size_xy;
    const auto block_lo = (int)start_tile.x * tile_size_xy + start_x + (int)bbox/2;
    const auto block_offset = Vector2b{(int8_t)(block_lo - block_x + block_size.x()/2), 0};

    auto w = world();
    add_floors(w, from, max_dist, bbox);
    add_block(w, block_tile, block_size, block_offset);
    auto& c = w[flush_ch];
    c.mark_passability_modified();
    c.ensure_passability();

    const auto& pred = Search::without_critters();
    auto A = astar{};
    auto pool = Grid::Pass::Pool{Grid::Pass::Params{bbox, bbox, pass_mode::pass}, pred};
    fm_assert(pool.params().div_size() == bbox);
    fm_assert(box_free(w, from, Vector2ui(bbox)));
    fm_assert(!box_free(w, point::normalize_coords(from, {1, 0}), Vector2ui(bbox)));
    fm_assert(!A.cache()->is_passable_for_bbox(w, pool, from, pred));

    auto r1 = A.Dijkstra<0, false>(w, pool, from, goal, max_dist, Vector2ui(bbox), pred);
    auto r2 = A.Dijkstra<0, true>(w, pool, from, goal, max_dist, Vector2ui(bbox), pred);
    if (!r1.is_found() || !r2.is_found())
        fm_abort("closed start cell at div %u: found %d, with closest %d", bbox, (int)r1.is_found(), (int)r2.is_found());
    fm_assert(r1.path().back() == goal);
    fm_assert(r2.cost() == r1.cost());
}

// A corridor two tiles long, exactly as wide as the union of an odd body's boxes over one cell.
// That cell column is the only way to the goal, and bbox + 1 closes it.
void test_odd_body_corridor(uint32_t bbox)
{
    constexpr uint32_t max_dist = 1024;
    constexpr auto t = (int)tile_size_xy;
    constexpr int8_t goal_x = -8;
    const auto from = point{global_coords{flush_ch, {8, 12}}, {goal_x, 0}};
    const auto goal = point{global_coords{flush_ch, flush_tile}, {goal_x, 0}};
    const auto& pred = Search::without_critters();
    auto A = astar{};
    const auto d = (int)A.pool_registry().pool_for(bbox).params().div_size();
    auto exact = Grid::Pass::Pool{Grid::Pass::Params{(uint32_t)d, bbox, pass_mode::pass}, pred};
    auto wider = Grid::Pass::Pool{Grid::Pass::Params{(uint32_t)d, bbox + 1, pass_mode::pass}, pred};
    fm_assert(exact.params().div_size() == (uint32_t)d && wider.params().div_size() == (uint32_t)d);

    const auto x = (int)flush_tile.x * t + goal_x;
    const auto p0 = (x + half_tile<int>) / d * d - half_tile<int>;
    const auto lo = (int)bbox/2, hi = (int)bbox - lo;
    const auto left = p0 - lo, right = p0 + d - 1 + hi;

    auto w = world();
    add_floors(w, from, max_dist, (uint32_t)d);
    add_block(w, {7, 8}, {tile_size_xy, 2*tile_size_xy}, {(int8_t)(left - (7*t + t/2)), 0});
    add_block(w, {9, 8}, {tile_size_xy, 2*tile_size_xy}, {(int8_t)(right - (9*t - t/2)), 0});
    auto& c = w[flush_ch];
    c.mark_passability_modified();
    c.ensure_passability();

    fm_assert(A.cache()->is_passable_for_bbox(w, exact, goal, pred));
    fm_assert(!A.cache()->is_passable_for_bbox(w, wider, goal, pred));

    auto r1 = A.Dijkstra<0, false>(w, exact, from, goal, max_dist, Vector2ui(bbox), pred);
    fm_assert(r1.is_found());
    auto r2 = A.Dijkstra<0, false>(w, from, goal, max_dist, Vector2ui(bbox), pred);
    if (!r2.is_found())
        fm_abort("odd body corridor bbox %u: registry pool has bbox %u", bbox, A.pool_registry().pool_for(bbox).params().bbox_size);
    fm_assert(r2.cost() == r1.cost());
}

// From chunk 0's east edge to goals in chunk 1's east half, max_dist barely covering each. Nodes
// then reach the far side of Search::cache's rectangle, and A* tests cells one chunk past it.
void test_cache_margin()
{
    constexpr uint32_t cell = 16;
    const auto from = point{global_coords{flush_ch, {15, 8}}, {31, 0}};
    const auto goal = [](uint8_t tx, int8_t ox) { return point{global_coords{{1, 0, 0}, {tx, 8}}, {ox, 0}}; };
    const auto max_dist = [&](point to) { return point::distance(from, to) + 7; };
    const auto& pred = Search::without_critters();
    auto A = astar{};
    auto pool = Grid::Pass::Pool{Grid::Pass::Params{cell, cell, pass_mode::pass}, pred};

    auto w = world();
    add_floors(w, from, max_dist(goal(TILE_MAX_DIM - 1, half_tile<int> - 1)), cell);

    for (uint8_t tx = 12; tx < TILE_MAX_DIM; tx++)
        for (int ox = -half_tile<int>; ox < half_tile<int>; ox++)
        {
            const auto to = goal(tx, (int8_t)ox);
            auto r = A.Dijkstra<0, false>(w, pool, from, to, max_dist(to), Vector2ui(cell), pred);
            if (!r.is_found())
                fm_abort("cache margin: goal tile %u offset %d not found", (unsigned)tx, ox);
        }
}

void add_pocket(world& w, point pt, uint32_t size, int open)
{
    constexpr int t = 16;
    fm_assert(pt.chunk3() == flush_ch);
    const auto h = (int)size/2;
    const auto o = Vector2i(pt.offset());
    const auto back = Vector2ub{(uint8_t)t, (uint8_t)(size + 2*t)}, side = Vector2ub{(uint8_t)(size + t), (uint8_t)t};
    add_block(w, pt.local(), back, Vector2b(o + Vector2i{-open*(h + t/2), 0}));
    add_block(w, pt.local(), side, Vector2b(o + Vector2i{-open*t/2, -(h + t/2)}));
    add_block(w, pt.local(), side, Vector2b(o + Vector2i{-open*t/2, h + t/2}));
}

void check_one_move(world& w, point pt, uint32_t size, Vector2i open)
{
    fm_assert(box_free(w, pt, Vector2ui(size)));
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx || dy)
                fm_assert(box_free(w, point::normalize_coords(pt, {dx, dy}), Vector2ui(size)) == (Vector2i{dx, dy} == open));
}

bptr<critter> add_walker(world& w, point from, uint32_t size)
{
    critter_proto p;
    p.name = "npc"_s;
    p.speed = 1;
    p.bbox_size = Vector2ub((uint8_t)size);
    p.offset = from.offset();
    auto C = w.make_object<critter>(w.make_id(), from.coord(), move(p));
    w.init_scripts();
    return C;
}

// The walk collides like any other move, so a path through a collider stops it short.
void walk_to(world& w, const bptr<critter>& C, path_search_result path, point goal, const char* name, uint32_t cell)
{
    C->script.do_reassign(critter_script::make_walk_script(move(path)), C);
    const auto frame = Second / C->atlas->info().fps;
    for (auto frames = 0u; C->last_walk == critter::walk_result::none; frames++)
    {
        fm_assert(frames < 4000);
        auto i = C->index();
        C->update(C, i, frame);
    }
    if (C->last_walk != critter::walk_result::arrived || C->position() != goal)
    {
        const auto at = Vector3i(C->position()), to = Vector3i(goal);
        fm_abort("%s at div %u: walk ended %d at (%d, %d), goal (%d, %d)",
                 name, cell, (int)C->last_walk, at.x(), at.y(), to.x(), to.y());
    }
    w.finish_scripts();
}

// The start stands in a pocket 3 px off the lattice on both axes, so its cell and every ring
// seed are closed and only its seed search gets out. With boxed_goal, the goal does too.
void check_pocket(uint32_t cell, bool boxed_goal)
{
    constexpr uint32_t max_dist = 1024, size = 32;
    const auto from = point{global_coords{flush_ch, {5, 8}}, {3, 3}};
    const auto goal = point{global_coords{flush_ch, {11, 8}}, {-5, 7}};
    const char* const name = boxed_goal ? "pocket, boxed goal" : "pocket";
    const auto& pred = Search::without_critters();
    auto A = astar{};
    auto pool = Grid::Pass::Pool{Grid::Pass::Params{cell, size, pass_mode::pass}, pred};
    fm_assert(pool.params().div_size() == cell);

    auto w = world();
    add_floors(w, from, max_dist, cell);
    add_pocket(w, from, size, -1);
    if (boxed_goal)
        add_pocket(w, goal, size, 1);
    // The seed search's flood passes the walker's own collider.
    const auto C = add_walker(w, from, size);
    auto& c = w[flush_ch];
    c.mark_passability_modified();
    c.ensure_passability();

    check_one_move(w, from, size, {-1, 0});
    fm_assert(!A.cache()->is_passable_for_bbox(w, pool, from, pred));
    if (boxed_goal)
    {
        check_one_move(w, goal, size, {1, 0});
        fm_assert(!A.cache()->is_passable_for_bbox(w, pool, goal, pred));
    }

    auto r1 = A.Dijkstra<0, false>(w, pool, from, goal, max_dist, Vector2ui(size), pred);
    auto r2 = A.Dijkstra<0, true>(w, pool, from, goal, max_dist, Vector2ui(size), pred);
    if (!r1.is_found() || !r2.is_found())
        fm_abort("%s at div %u: found %d, with closest %d", name, cell, (int)r1.is_found(), (int)r2.is_found());
    fm_assert(r1.path().front() == from && r1.path().back() == goal);
    fm_assert(r2.cost() == r1.cost());
    // Leaving a pocket goes a box width away from the other end and back. No step costs less
    // than its x travel.
    const auto x_travel = (uint32_t)(Vector3i(goal).x() - Vector3i(from).x()) + 2*size*(1 + (uint32_t)boxed_goal);
    if (r1.cost() < x_travel)
        fm_abort("%s at div %u: cost %u, the way around is at least %u", name, cell, r1.cost(), x_travel);
    walk_to(w, C, move(r1), goal, name, cell);
}

// Every seed costs max_dist or more. From the pocket, its seed search first meets a set cell a
// box width out. Toward it, the ring seeds around an off-lattice start lie 1 px or more away.
void check_seeds_past_max_dist(uint32_t cell)
{
    constexpr uint32_t size = 32;
    const auto pocket = point{global_coords{flush_ch, {5, 8}}, {3, 3}};
    const auto open = point{global_coords{flush_ch, {11, 8}}, {-5, 7}};
    const auto& pred = Search::without_critters();
    auto A = astar{};
    auto pool = Grid::Pass::Pool{Grid::Pass::Params{cell, size, pass_mode::pass}, pred};

    auto w = world();
    add_floors(w, pocket, 1024, cell);
    add_pocket(w, pocket, size, -1);
    auto& c = w[flush_ch];
    c.mark_passability_modified();
    c.ensure_passability();
    fm_assert(!A.cache()->is_passable_for_bbox(w, pool, pocket, pred));

    struct search { point from, to; uint32_t max_dist; };
    for (auto [from, to, max_dist] : { search{pocket, open, size}, search{open, pocket, 1} })
    {
        auto r1 = A.Dijkstra<0, false>(w, pool, from, to, max_dist, Vector2ui(size), pred);
        auto r2 = A.Dijkstra<0, true>(w, pool, from, to, max_dist, Vector2ui(size), pred);
        if (r1.is_found() || r2.is_found() || !r1.path().isEmpty() || !r2.path().isEmpty())
            fm_abort("seeds past max_dist %u at div %u: found %d, with closest %d, path lengths %zu %zu",
                     max_dist, cell, (int)r1.is_found(), (int)r2.is_found(), r1.path().size(), r2.path().size());
    }
}

// The goal stands in a box-wide corridor 5 px short of the room it opens into, so its cell is
// closed and room nodes lie near it. The room's only other way in is the corridor's far end,
// facing away from the start. A* exhausts without reaching the goal, then the goal's seed search
// runs down the corridor to its nodes.
void check_corridor_goal(uint32_t cell)
{
    constexpr uint32_t max_dist = 1024, size = 32;
    constexpr auto room_tile = local_coords{10, 8};
    constexpr int corridor_west = 576;
    const auto from = point{global_coords{flush_ch, {13, 8}}, {}};
    const auto goal = point{global_coords{flush_ch, room_tile}, {5, 0}};
    const auto& pred = Search::without_critters();
    auto A = astar{};
    auto pool = Grid::Pass::Pool{Grid::Pass::Params{cell, size, pass_mode::pass}, pred};

    auto w = world();
    add_floors(w, from, max_dist, cell);
    add_block(w, room_tile, {64, 64}, {-32, -48});
    add_block(w, room_tile, {64, 64}, {-32, 48});
    add_block(w, room_tile, {112, 16}, {56, -72});
    add_block(w, room_tile, {112, 16}, {56, 72});
    add_block(w, room_tile, {16, 128}, {104, 0});
    const auto C = add_walker(w, from, size);
    auto& c = w[flush_ch];
    c.mark_passability_modified();
    c.ensure_passability();

    fm_assert(box_free(w, goal, Vector2ui(size)));
    fm_assert(!box_free(w, point::normalize_coords(goal, {0, -1}), Vector2ui(size)));
    fm_assert(!box_free(w, point::normalize_coords(goal, {0, 1}), Vector2ui(size)));
    fm_assert(!A.cache()->is_passable_for_bbox(w, pool, goal, pred));

    auto r1 = A.Dijkstra<0, false>(w, pool, from, goal, max_dist, Vector2ui(size), pred);
    auto r2 = A.Dijkstra<0, true>(w, pool, from, goal, max_dist, Vector2ui(size), pred);
    if (!r1.is_found() || !r2.is_found())
        fm_abort("corridor goal at div %u: found %d, with closest %d", cell, (int)r1.is_found(), (int)r2.is_found());
    fm_assert(r1.path().front() == from && r1.path().back() == goal);
    fm_assert(r2.cost() == r1.cost());
    // The corridor is entered where the box clears its walls.
    const auto mouth_x = corridor_west - (int)size/2;
    const auto x_travel = (uint32_t)(Vector3i(from).x() - mouth_x + Vector3i(goal).x() - mouth_x);
    if (r1.cost() < x_travel)
        fm_abort("corridor goal at div %u: cost %u, the way around is at least %u", cell, r1.cost(), x_travel);
    walk_to(w, C, move(r1), goal, "corridor goal", cell);
}

// Posts 3 px apart past the box close every cell between them at div 8. In the gaps, the flood
// reaches some positions diagonally first, then cheaper on an axial step.
void check_post_field()
{
    constexpr uint32_t max_dist = 1024, size = 32, cell = 8;
    constexpr int post = 16, pitch = post + (int)size - 1 + 3;
    const auto from = point{global_coords{flush_ch, flush_tile}, {pitch/2, pitch/2}};
    const auto goal = point{global_coords{flush_ch, {12, 8}}, {}};
    const auto& pred = Search::without_critters();
    auto A = astar{};
    auto pool = Grid::Pass::Pool{Grid::Pass::Params{cell, size, pass_mode::pass}, pred};

    auto w = world();
    add_floors(w, from, max_dist, cell);
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
            add_block(w, flush_tile, Vector2ub(post), Vector2b{(int8_t)(i*pitch), (int8_t)(j*pitch)});
    const auto C = add_walker(w, from, size);
    auto& c = w[flush_ch];
    c.mark_passability_modified();
    c.ensure_passability();
    fm_assert(!A.cache()->is_passable_for_bbox(w, pool, from, pred));

    auto r1 = A.Dijkstra<0, false>(w, pool, from, goal, max_dist, Vector2ui(size), pred);
    auto r2 = A.Dijkstra<0, true>(w, pool, from, goal, max_dist, Vector2ui(size), pred);
    if (!r1.is_found() || !r2.is_found())
        fm_abort("post field: found %d, with closest %d", (int)r1.is_found(), (int)r2.is_found());
    fm_assert(r1.path().front() == from && r1.path().back() == goal);
    fm_assert(r2.cost() == r1.cost());

    walk_to(w, C, move(r1), goal, "post field", cell);
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
    constexpr auto own_size = Vector2ui{16};
    auto& pool = A.pool_registry().pool_for(own_size.x());

    add_floors(w, from, max_dist, pool.params().div_size());
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

    const auto goal = point{wpos, {wox, woy}};
    const auto& pred = Search::without_critters();

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
                    if (!A.cache()->is_passable_for_bbox(w, pool, pt, pred) && box_free(w, pt, own_size))
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

    for (auto bbox : { 32u, 31u, 33u })
        test_flush_goals(bbox);
    check_closed_start_cell(32, -2);
    check_closed_start_cell(64, 10);
    for (auto bbox : { 31u, 33u })
        test_odd_body_corridor(bbox);
    test_cache_margin();
    for (auto cell : { 8u, 16u, 32u })
    {
        check_pocket(cell, false);
        check_pocket(cell, true);
        check_seeds_past_max_dist(cell);
        check_corridor_goal(cell);
    }
    check_post_field();
}

} // namespace floormat
