#include "app.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/critter-script-cover.hpp"
#include "src/critter-script-cover-spot-tests.hpp"
#include "src/anim-atlas.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-order.hpp"
#include "src/grid-registry.hpp"
#include "src/order-list.inl"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/search-astar.hpp"
#include "src/tile-image.hpp"
#include "src/update-world.hpp"
#include "src/wall-atlas.hpp"
#include "src/point.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/nanosecond.inl"
#include "compat/thread-pool.hpp"
#include <array>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>

namespace floormat::Test {

namespace {

using state = Cover::Script::status::state;

constexpr auto chunk_a = chunk_coords_{0, 0, 0};
// test_cover_npcs_think_order's npc body, cover grid size, threat and npc positions.
constexpr auto body = Vector2ui{30, 24};
constexpr u32 cover_div_size = 8, num_cover_npcs = 4;
constexpr point npc_positions[] = {
    {chunk_a, {4, 7}, {}}, {chunk_a, {4, 9}, {}}, {chunk_a, {3, 8}, {}}, {chunk_a, {5, 10}, {}},
};
constexpr auto threat_start = point{chunk_a, {12, 8}, {}};
// South-west of the npcs' cover, where the threat sees them.
constexpr auto threat_moved = point{chunk_a, {2, 14}, {}};
constexpr u32 max_frames_per_phase = 3600;

bptr<critter> add_critter(world& w, point pos, Vector2ui size = body, object_id id = 0)
{
    critter_proto p;
    p.name = "npc"_s;
    p.bbox_size = Vector2ub(size);
    p.offset = pos.offset();
    return w.make_object<critter>(id ? id : w.make_id(), pos.coord(), move(p));
}

void add_ground(world& w, chunk_coords_ ch)
{
    auto& c = w[ch];
    const auto ground = tile_image_proto{loader.ground_atlas("tiles"), 0};
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = ground;
}

bptr<scenery> add_wall(world& w, local_coords tile, Vector2ub bbox_size)
{
    scenery_proto p;
    p.atlas = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype = generic_scenery_proto{};
    p.bbox_size = bbox_size;
    p.pass = pass_mode::blocked;
    return w.make_scenery(w.make_id(), {chunk_a, tile}, move(p));
}

// test_cover_npcs_think_order's two walls in chunk_a, with the chunk's passability rebuilt.
void add_think_order_walls(world& w)
{
    add_ground(w, chunk_a);
    add_wall(w, {7, 8}, {16, 96});
    add_wall(w, {7, 12}, {16, 96});
    auto& c = w[chunk_a];
    c.mark_passability_modified();
    c.ensure_passability();
}

bool is_hidden_or_retreating(state st)
{
    return st == state::hidden || st == state::retreat || st == state::find_retreat;
}

// Advances w by one frame in which npcs[k] thinks under update_index k, in that order or reversed, inside
// pool->parallel_for() with a pool, with astars[(w.frame_no() + k) % astars.size()] if any. Then the orders run.
template<typename F>
void run_frame(world& w, ArrayView<const bptr<critter>> npcs, Ns dt, ThreadPool* pool,
               ArrayView<astar* const> astars, bool is_reverse_order, F&& after_think)
{
    fm_assert(w.thread_pool() == pool);
    w.increment_frame_no();
    w.advance_game_time(dt);
    prepare_world_for_thinking(w);
    auto& buffer = w.frame_order_buffer();
    auto& list = w.frame_order_list();
    auto ctx = think_context{buffer, w.grid_orders()};
    const auto& reg = *w.grid_registry();
    for (auto n = 0u; n < npcs.size(); n++)
    {
        const auto k = is_reverse_order ? (u32)npcs.size() - 1 - n : n;
        ctx.astar = astars.isEmpty() ? nullptr : astars[(w.frame_no() + k) % astars.size()];
        const auto num_pools = reg.pool_count();
        const auto think = [&](u32, u32, u32) { npcs[k]->think(dt, ctx, k); };
        if (pool)
            pool->parallel_for(1, 1, think);
        else
            think(0, 1, 0);
        fm_assert_equal(num_pools, reg.pool_count());
        after_think(k);
    }
    list.append_buffers({&buffer, 1});
    list.run_all([&](const order& o, u32) { run_order(w, o, dt); });
    list.clear();
    buffer.clear();
}

// test_cover_npcs_think_order's world: a threat east of two walls and cover npcs west of them. With
// has_grids_built, every grid the npcs read and every octant of the cover grid is built before the first frame.
struct cover_npcs_world
{
    astar A;
    world w;
    bptr<critter> threat;
    Array<bptr<critter>> npcs;

    explicit cover_npcs_world(bool has_grids_built, u32 num_npcs = num_cover_npcs);
    ~cover_npcs_world() noexcept;
    Cover::Script::status status(u32 k);
    void move_threat(point pos);
    void run_frame(Ns dt, ThreadPool* pool, ArrayView<astar* const> astars, bool is_reverse_order = false);
    Ns frame() const;
};

cover_npcs_world::cover_npcs_world(bool has_grids_built, u32 num_npcs)
{
    add_think_order_walls(w);
    threat = add_critter(w, threat_start);
    arrayReserve(npcs, num_npcs);
    for (auto k = 0u; k < num_npcs; k++)
        arrayAppend(npcs, add_critter(w, npc_positions[k]));
    w.set_grid_registry(&A.pool_registry());
    w.init_scripts();
    for (const auto& npc : npcs)
        npc->script.do_reassign(critter_script::make_cover_script(A, threat->id, cover_div_size), npc);
    if (has_grids_built)
    {
        auto& reg = A.pool_registry();
        const auto side = Math::max(body.x(), body.y());
        auto& objects = reg.object_pool(Grid::Pass::Params{astar::default_div_size, side, pass_mode::pass});
        auto& cover = reg.cover_pool(Grid::Cover::Params{cover_div_size});
        auto& rays = reg.pass_pool(Grid::Pass::Params{cover_div_size, pass_mode::shoot_through});
        for (auto& ch : w.chunks())
        {
            objects[ch].build_if_stale();
            objects.pass_pool()[ch].build_if_stale();
            rays[ch].build_if_stale();
            auto g = cover[ch];
            g.build_if_stale();
            for (auto k = 0u; k < Grid::Cover::octant_count; k++)
                (void)g.ensure_octant(k);
            fm_assert_equal((u32)-1, g.built_octants());
        }
    }
}

cover_npcs_world::~cover_npcs_world() noexcept { w.finish_scripts(); }

Cover::Script::status cover_npcs_world::status(u32 k)
{
    const auto s = Cover::Script::get_status(*npcs[k]);
    fm_assert(s);
    return *s;
}

void cover_npcs_world::move_threat(point pos)
{
    auto i = threat->index();
    threat->teleport_to(i, pos, rotation_COUNT);
}

void cover_npcs_world::run_frame(Ns dt, ThreadPool* pool, ArrayView<astar* const> astars, bool is_reverse_order)
{
    Test::run_frame(w, npcs, dt, pool, astars, is_reverse_order, [](u32) {});
}

Ns cover_npcs_world::frame() const { return Second / npcs[0]->atlas->info().fps; }

// Returns true once each npc of s was hidden or retreating after one of the frames since `reached` was cleared.
bool has_each_npc_hidden_or_retreated(cover_npcs_world& s, Array<bool>& reached)
{
    auto is_done = true;
    for (auto k = 0u; k < s.npcs.size(); k++)
    {
        reached[k] = reached[k] || is_hidden_or_retreating(s.status(k).st);
        is_done = is_done && reached[k];
    }
    return is_done;
}

// Calls run_one_frame() until each npc of s hid or retreated, then move_threat_to(threat_moved), then
// run_one_frame() until each did again. Each npc hidden when the threat moved must have searched again.
template<typename F, typename G>
void run_threat_move_phases(cover_npcs_world& s, F&& run_one_frame, G&& move_threat_to)
{
    Array<bool> reached{ValueInit, s.npcs.size()};
    for (auto n = 0u; ; n++)
    {
        fm_assert(n < max_frames_per_phase);
        run_one_frame();
        if (has_each_npc_hidden_or_retreated(s, reached))
            break;
    }
    Array<bool> was_hidden{ValueInit, s.npcs.size()};
    Array<u32> num_searches{ValueInit, s.npcs.size()};
    auto num_hidden = 0u;
    for (auto k = 0u; k < s.npcs.size(); k++)
    {
        const auto t = s.status(k);
        was_hidden[k] = t.st == state::hidden;
        num_hidden += was_hidden[k];
        num_searches[k] = t.counters.num_searches;
        reached[k] = false;
    }
    fm_assert(num_hidden > 0);
    move_threat_to(threat_moved);
    for (auto n = 0u; ; n++)
    {
        fm_assert(n < max_frames_per_phase);
        run_one_frame();
        if (has_each_npc_hidden_or_retreated(s, reached))
            break;
    }
    for (auto k = 0u; k < s.npcs.size(); k++)
        if (was_hidden[k])
        {
            const auto t = s.status(k);
            fm_assert(t.counters.num_searches > num_searches[k] || t.search.tiles_left != 0);
        }
}

// Calls run_one_frame() for test_cover_npcs_think_order's 900 frames, and move_threat_to() where that test moves
// the threat.
template<typename F, typename G>
void run_think_order_frames(F&& run_one_frame, G&& move_threat_to)
{
    const point threat_positions[] = { {chunk_a, {12, 4}, {}}, {chunk_a, {12, 13}, {}} };
    for (auto n = 0u; n < 900; n++)
    {
        if (n % 300 == 299)
            move_threat_to(threat_positions[n / 300 % 2]);
        run_one_frame();
    }
}

void test_think_order_world_in_thread_pool(ThreadPool& pool)
{
    cover_npcs_world plain{true}, threaded{true};
    astar B{threaded.A.pool_registry()};
    astar* const astars[] = {&B};
    threaded.w.set_thread_pool(&pool);
    const auto frame = plain.frame();
    run_think_order_frames([&] { plain.run_frame(frame, nullptr, {}); }, [&](point p) { plain.move_threat(p); });
    run_think_order_frames([&] { threaded.run_frame(frame, &pool, astars); },
                           [&](point p) { threaded.move_threat(p); });
    for (auto k = 0u; k < num_cover_npcs; k++)
    {
        const auto &a = *plain.npcs[k], &b = *threaded.npcs[k];
        fm_assert(a.coord == b.coord && a.offset == b.offset && a.r == b.r);
    }
}

void test_world_without_grids_in_thread_pool(ThreadPool& pool)
{
    cover_npcs_world s{false};
    astar B{s.A.pool_registry()};
    astar* const astars[] = {&B};
    s.w.set_thread_pool(&pool);
    const auto frame = s.frame();
    run_threat_move_phases(s, [&] { s.run_frame(frame, &pool, astars); }, [&](point p) { s.move_threat(p); });
}

// A wall spans chunk_a from north to south, with the threat and the npc west of it, and no other chunk exists.
// Every path to cover behind the wall leaves chunk_a, and no think may create the chunk it crosses.
void test_path_through_missing_chunk(ThreadPool& pool)
{
    astar A;
    astar B{A.pool_registry()};
    astar* const astars[] = {&B};
    world w;
    add_ground(w, chunk_a);
    for (const auto y : { u8{1}, u8{4}, u8{7}, u8{10}, u8{13} })
        add_wall(w, {10, y}, {16, 192});
    add_wall(w, {10, 15}, {16, 64});
    auto& c = w[chunk_a];
    c.mark_passability_modified();
    c.ensure_passability();
    const auto threat = add_critter(w, point{chunk_a, {2, 8}, {}});
    // A square body, as a {30, 24} one stopped below the wall's south end and never reached its cover spot.
    const auto npc = add_critter(w, point{chunk_a, {8, 13}, {}}, Vector2ui{24});
    w.set_grid_registry(&A.pool_registry());
    w.init_scripts();
    npc->script.do_reassign(critter_script::make_cover_script(A, threat->id, cover_div_size), npc);
    w.set_thread_pool(&pool);
    const bptr<critter> npcs[] = {npc};
    const auto frame = Second / npc->atlas->info().fps;
    std::array<bool, 9> has_chunk_before_think{};
    const auto has_chunk = [&](u32 i) { return !!w.at(chunk_a + Vector2i{(int)(i % 3) - 1, (int)(i / 3) - 1}); };
    for (auto n = 0u; Cover::Script::get_status(*npc)->st != state::hidden || n == 0; n++)
    {
        fm_assert(n < max_frames_per_phase);
        for (auto i = 0u; i < 9; i++)
            has_chunk_before_think[i] = has_chunk(i);
        run_frame(w, npcs, frame, &pool, astars, false, [&](u32) {
            for (auto i = 0u; i < 9; i++)
                fm_assert(has_chunk(i) == has_chunk_before_think[i]);
        });
    }
    fm_assert(Vector3i(npc->position()).x() > 10*tile_size_xy);
    // Chunks 1 and 7 lie north and south of chunk_a. The npc's orders created the one it walked through.
    fm_assert(has_chunk(1) || has_chunk(7));
    w.finish_scripts();
}

// Asserts that a and b hold the same npc positions and script states, cover pool octant fills, and octants of
// each built cover grid.
void assert_same_state(cover_npcs_world& a, cover_npcs_world& b)
{
    for (auto k = 0u; k < a.npcs.size(); k++)
    {
        const auto &x = *a.npcs[k], &y = *b.npcs[k];
        fm_assert(x.coord == y.coord && x.offset == y.offset && x.r == y.r);
        fm_assert(a.status(k).st == b.status(k).st);
    }
    const auto params = Grid::Cover::Params{cover_div_size};
    const auto* pa = a.A.pool_registry().find_cover_pool(params);
    const auto* pb = b.A.pool_registry().find_cover_pool(params);
    fm_assert(!pa == !pb);
    if (!pa)
        return;
    fm_assert_equal(pa->octant_fills(), pb->octant_fills());
    for (const auto& c : a.w.chunks())
    {
        const auto* cb = b.w.at(c.coord());
        fm_assert(cb);
        const auto ga = pa->find_built_grid(c), gb = pb->find_built_grid(*cb);
        fm_assert(!ga == !gb);
        if (ga)
            fm_assert_equal(ga.built_octants(), gb.built_octants());
    }
}

void test_think_order_without_grids(ThreadPool& pool)
{
    cover_npcs_world s1{false}, s2{false};
    astar B1{s1.A.pool_registry()}, B2{s2.A.pool_registry()};
    astar* const astars1[] = {&B1};
    astar* const astars2[] = {&B2};
    s1.w.set_thread_pool(&pool);
    s2.w.set_thread_pool(&pool);
    const auto frame = s1.frame();
    run_threat_move_phases(s1, [&] {
        s1.run_frame(frame, &pool, astars1, false);
        s2.run_frame(frame, &pool, astars2, true);
        assert_same_state(s1, s2);
    }, [&](point p) { s1.move_threat(p); s2.move_threat(p); });
}

// Runs the frame in which the npc finds a pool missing: it gives no order and one or more grid orders.
void run_frame_waiting_for_pool(world& w, ArrayView<const bptr<critter>> npcs, Ns dt, ThreadPool& pool,
                                ArrayView<astar* const> astars)
{
    fm_assert_equal(1u, (u32)npcs.size());
    run_frame(w, npcs, dt, &pool, astars, false, [&](u32) {
        fm_assert(w.frame_order_buffer().empty());
        fm_assert(!w.grid_orders().isEmpty());
    });
}

void run_frames_until_hidden(world& w, ArrayView<const bptr<critter>> npcs, Ns dt, ThreadPool& pool,
                             ArrayView<astar* const> astars)
{
    for (auto n = 0u; Cover::Script::get_status(*npcs[0])->st != state::hidden; n++)
    {
        fm_assert(n < max_frames_per_phase);
        run_frame(w, npcs, dt, &pool, astars, false, [](u32) {});
    }
}

void test_div_size_changed_mid_walk(ThreadPool& pool)
{
    cover_npcs_world s{false, 1};
    astar B{s.A.pool_registry()};
    astar* const astars[] = {&B};
    s.w.set_thread_pool(&pool);
    const auto frame = s.frame();
    for (auto n = 0u; s.status(0).st != state::to_cover; n++)
    {
        fm_assert(n < max_frames_per_phase);
        s.run_frame(frame, &pool, astars);
    }
    const auto params = Grid::Cover::Params{2*cover_div_size};
    const auto& reg = s.A.pool_registry();
    fm_assert(!reg.find_cover_pool(params));
    Cover::Script::set_options(*s.npcs[0], {.div_size = params.div_size});
    fm_assert(reg.find_cover_pool(params));
    // run_frame() asserts that no think changes reg.pool_count().
    run_frame(s.w, s.npcs, frame, &pool, astars, false, [&](u32) {
        for (const auto& o : s.w.grid_orders())
            switch (o.kind)
            {
            case grid_order::grid_kind::pass:
                fm_assert(reg.find_pass_pool(o.pass_params));
                break;
            case grid_order::grid_kind::object:
                fm_assert(reg.find_object_pool(o.pass_params));
                break;
            case grid_order::grid_kind::cover:
                fm_assert(reg.find_cover_pool(Grid::Cover::Params{o.cover_div_size}));
                break;
            }
        fm_assert(!s.w.frame_order_buffer().empty());
    });
    run_frames_until_hidden(s.w, s.npcs, frame, pool, astars);
}

void test_threat_made_after_init(ThreadPool& pool)
{
    astar A;
    astar B{A.pool_registry()};
    astar* const astars[] = {&B};
    world w;
    add_think_order_walls(w);
    const auto threat_id = w.make_id();
    const auto npc = add_critter(w, npc_positions[0]);
    w.set_grid_registry(&A.pool_registry());
    w.init_scripts();
    npc->script.do_reassign(critter_script::make_cover_script(A, threat_id, cover_div_size), npc);
    w.set_thread_pool(&pool);
    const bptr<critter> npcs[] = {npc};
    const auto frame = Second / npc->atlas->info().fps;
    for (auto n = 0u; n < 4; n++)
        run_frame(w, npcs, frame, &pool, astars, false, [](u32) {});
    // A threat as big as the npc would read the npc's own object pool.
    constexpr auto threat_size = Vector2ui{40, 40};
    const auto params = Grid::Object::Params{astar::default_div_size, threat_size.x(), pass_mode::pass};
    const auto& reg = A.pool_registry();
    fm_assert(!reg.find_object_pool(params));
    (void)add_critter(w, threat_start, threat_size, threat_id);
    run_frame_waiting_for_pool(w, npcs, frame, pool, astars);
    run_frame(w, npcs, frame, &pool, astars, false, [&](u32) { fm_assert(reg.find_object_pool(params)); });
    run_frames_until_hidden(w, npcs, frame, pool, astars);
    w.finish_scripts();
}

void test_spot_tests_with_given_astar()
{
    cover_npcs_world s{true, 1};
    astar B{s.A.pool_registry()};
    auto& npc = *s.npcs[0];
    Cover::spot_tests t{s.A, s.threat->id, 0};
    t.create_pools(npc, {.div_size = cover_div_size});
    // East of the wall at {7, 8}, which stands between it and the npc.
    const auto to = point{chunk_a, {9, 8}, {}};
    prepare_world_for_thinking(s.w);
    auto ctx = think_context{s.w.frame_order_buffer(), s.w.grid_orders(), &B};
    fm_assert(t.path_to(npc, to, ctx).is_found());
    fm_assert_equal(0u, s.A.last_node_count());
    fm_assert(B.last_node_count() > 0);
    fm_assert_equal(B.last_node_count(), t.last_dijkstra_node_count());
    fm_assert(t.is_reachable_without_critters(npc, to, ctx));
    fm_assert_equal(0u, s.A.last_node_count());
    fm_assert(B.last_node_count() > 0);
    fm_assert_equal(B.last_node_count(), t.last_dijkstra_node_count());
    ctx.astar = nullptr;
    fm_assert(t.path_to(npc, to, ctx).is_found());
    fm_assert(s.A.last_node_count() > 0);
    fm_assert_equal(s.A.last_node_count(), t.last_dijkstra_node_count());
    fm_assert(t.is_reachable_without_critters(npc, to, ctx));
    fm_assert_equal(s.A.last_node_count(), t.last_dijkstra_node_count());
}

// The npcs decide the same whether each think searches with the scripts' astar, with one other astar, or with
// one of two others picked by frame and npc.
void test_think_order_with_given_astars(ThreadPool& pool)
{
    cover_npcs_world s1{false}, s2{false}, s3{false};
    astar B2{s2.A.pool_registry()}, B3{s3.A.pool_registry()}, C3{s3.A.pool_registry()};
    astar* const astars2[] = {&B2};
    astar* const astars3[] = {&B3, &C3};
    const astar* const given_astars[] = {&B2, &B3, &C3};
    std::array<bool, 3> has_searched{};
    s2.w.set_thread_pool(&pool);
    s3.w.set_thread_pool(&pool);
    const auto frame = s1.frame();
    run_think_order_frames([&] {
        s1.run_frame(frame, nullptr, {});
        s2.run_frame(frame, &pool, astars2);
        s3.run_frame(frame, &pool, astars3);
        assert_same_state(s1, s2);
        assert_same_state(s1, s3);
        for (auto i = 0u; i < has_searched.size(); i++)
            has_searched[i] = has_searched[i] || given_astars[i]->last_node_count() > 0;
    }, [&](point p) { s1.move_threat(p); s2.move_threat(p); s3.move_threat(p); });
    fm_assert_equal(0u, s2.A.last_node_count());
    fm_assert_equal(0u, s3.A.last_node_count());
    fm_assert(has_searched[0] && has_searched[1] && has_searched[2]);
}

} // namespace

void test_think_read_only()
{
    ThreadPool pool{1};
    test_think_order_world_in_thread_pool(pool);
    test_world_without_grids_in_thread_pool(pool);
    test_path_through_missing_chunk(pool);
    test_think_order_without_grids(pool);
    test_div_size_changed_mid_walk(pool);
    test_threat_made_after_init(pool);
    test_spot_tests_with_given_astar();
    test_think_order_with_given_astars(pool);
}

} // namespace floormat::Test
