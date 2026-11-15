#include "cover-world.hpp"
#include "compat/debug.hpp"
#include "compat/assert.hpp"
#include "compat/function-view.hpp"
#include "compat/constantly.hpp"
#include "compat/nanosecond.inl"
#include "compat/borrowed-ptr.inl"
#include "src/anim-atlas.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/critter-script-cover.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-registry.hpp"
#include "src/raycast.hpp"
#include "src/search.hpp"
#include "src/search-astar.hpp"
#include "src/search-pred.hpp"
#include "src/world.hpp"
#include "src/wall-atlas.hpp"
#include "src/log.hpp"
#include "src/point.inl"
#include "src/intra-coord.inl"
#include "loader/loader.hpp"
#include <bit>
#include <cr/Optional.h>
#include <mg/Functions.h>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

namespace fm_debug = floormat::debug::detail;
using enum rotation;
using Function = function_view<Ns() const>;

critter_proto make_proto(float accel)
{
    critter_proto proto;
    proto.atlas = loader.anim_atlas("npc-walk", loader.ANIM_PATH);
    proto.name = "Player"_s;
    proto.speed = accel;
    proto.playable = true;
    proto.offset = {};
    proto.bbox_offset = {};
    proto.bbox_size = Vector2ub(tile_size_xy/2);
    return proto;
}

void mark_all_modified(world& w)
{
    for (auto& ch : w.chunks())
        ch.mark_modified();
}

struct Start
{
    StringView name, instance;
    point pt;
    double accel = 1;
    enum rotation rotation = N;
#if 1
    static constexpr bool quiet = true, verbose = false, silent = true;
#elif 1
    bool quiet = !is_log_verbose();
    bool verbose = false;
#elif 0
    bool verbose = true;
    bool quiet = false;
#elif 1
    bool verbose = false;
    bool quiet = false;
#elif 0
    bool quiet = is_log_quiet();
    bool verbose = is_log_standard() || is_log_verbose();
#else
    bool quiet = is_log_quiet() || is_log_standard();
    bool verbose = is_log_verbose();
#endif
};

struct Expected
{
    point pt;
    Ns time;
};

struct Grace
{
    Ns time = Ns{250};
    u32 distance_L1 = 24;
    static constexpr bool no_crash = true;
};

bool run_(critter& npc, const function_view<Ns() const>& make_dt,
          Start start, Expected expected, Grace grace)
{
    constexpr auto max_time = 300*Second;
    constexpr u32 max_steps = 800;

    fm_assert(grace.time != Ns{});
    fm_assert(!start.quiet | !start.verbose);
    //validate_start(start);
    //validate_expected(expected);
    //validate_grace(grace);
    fm_assert(start.accel > 1e-8);
    fm_assert(start.accel <= 50);
    fm_assert(start.name);
    fm_assert(start.instance);
    fm_assert(start.rotation < rotation_COUNT);
    expected.time.stamp = u64(expected.time.stamp / start.accel);
    fm_assert(expected.time <= max_time);
    fm_assert(grace.distance_L1 <= (u32)chunk_size<Vector2>.length());

    auto index = npc.index();
    npc.teleport_to(index, start.pt, rotation_COUNT);

    Ns time{0}, saved_time{0};
    auto last_pos = npc.position();
    u32 i;
    constexpr auto max_stop_frames = 250; // todo detect collisions properly and don't rely on this
    u32 frames_stopped = 0;

    if (!start.quiet) [[unlikely]]
        Debug{} << "**" << start.name << start.instance << nospace(":");

    constexpr auto print_pos = [](StringView prefix, point start, point pos, Ns time, Ns dt, const critter& npc) {
        DBG_nospace << prefix
                    << " " << pos
                    << " time:" << time
                    << " dt:" << dt
                    << " dist:" << point::distance_l1(pos, start)
                    << " delta:" << npc.delta;
    };

    auto fail = [b = grace.no_crash](const char* file, int line, const char* function) {
        if (b) [[likely]]
            return false;
        else
            fm_debug::emit_abort(file, line, function, "false");
    };

    for (i = 0; true; i++)
    {
        const auto dt = Ns{make_dt()};
        if (dt == Ns{}) [[unlikely]]
        {
            if (start.verbose) [[unlikely]]
                Debug{} << "| dt == 0, breaking";
            break;
        }
        if (start.verbose) [[unlikely]]
            print_pos("  ", expected.pt, npc.position(), time, dt, npc);
        fm_assert(dt >= Millisecond*1e-1);
        fm_assert(dt <= Second * 1000);
        npc.update_movement(index, dt, start.rotation);
        const auto pos = npc.position();
        const bool same_pos = pos == last_pos;
        last_pos = pos;

        time += dt;

        if (same_pos)
        {
            frames_stopped++;
            if (frames_stopped >= max_stop_frames) [[unlikely]]
            {
                if (!start.quiet) [[unlikely]]
                {
                    print_pos("->", expected.pt, pos, time, dt, npc);
                    DBG_nospace << "===>"
                        << " iters,"
                        << " time:" << time
                        << " distance:" << point::distance_l1(last_pos, expected.pt) << " px"
                        << Debug::newline;
                }
                break;
            }
        }
        else
        {
            frames_stopped = 0;
            saved_time = time;
        }

        if (time > max_time) [[unlikely]]
        {
            if (!start.quiet) [[unlikely]]
                print_pos("*", start.pt, last_pos, time, dt, npc);
            Error{standard_error()} << "!!! fatal: timeout" << max_time << "reached!";
            return fail(__FILE__, __LINE__, FM_PRETTY_FUNCTION);
        }
        if (i > max_steps) [[unlikely]]
        {
            if (!start.quiet) [[unlikely]]
                print_pos("*", start.pt, last_pos, time, dt, npc);
            if (!start.silent)
                Error{ standard_error()} << "!!! fatal: position doesn't converge after" << i << "iterations!";
            return fail(__FILE__, __LINE__, FM_PRETTY_FUNCTION);
        }
    }

    if (const auto dist_l1 = point::distance_l1(last_pos, expected.pt);
        dist_l1 > grace.distance_L1) [[unlikely]]
    {
        Error{standard_error()} << "!!! fatal: distance" << dist_l1 << "pixels" << "over grace distance of" <<  grace.distance_L1;
        return fail(__FILE__, __LINE__, FM_PRETTY_FUNCTION);
    }
    else if (start.verbose) [[unlikely]]
        Debug{} << "*" << "distance:" << dist_l1 << "pixels";

    if (expected.time != Ns{}) [[likely]]
    {
        const auto time_diff = Ns{Math::abs((i64)expected.time.stamp - (i64)saved_time.stamp)};
        if (time_diff > grace.time)
        {
            Error{ standard_error(), Debug::Flag::NoSpace }
                << "!!! fatal: wrong time " << saved_time
                << " expected:" << expected.time
                << " diff:" << time_diff
                << " for " << start.name << "/" << start.instance;
            return fail(__FILE__, __LINE__, FM_PRETTY_FUNCTION);
        }
    }

    return true;
}

bool run(world& w, const function_view<Ns() const>& make_dt,
         Start start, Expected expected, Grace grace = {})
{
    mark_all_modified(w);
    object_id id = 0;
    auto npc = w.ensure_player_character(id, make_proto((float)start.accel));
    bool ret = run_(*npc, make_dt, start, expected, grace);
    // otherwise the next run's ensure_player_character() returns this critter and ignores its proto
    auto& c = npc->chunk();
    c.sort_objects();
    // kill_object() would tear down a script these worlds never initialized
    c.remove_object(*npc, npc->index());
    npc.destroy();
    return ret;
}

world make_world1()
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{0,0,0}][{8,9}].wall_north() = W;
    w[{0,1,0}][{8,0}].wall_north() = W;
    return w;
}

world make_world2()
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{-1,-1,0}][{13,13}].wall_north() = W;
    w[{-1,-1,0}][{13,13}].wall_west() = W;
    w[{1,1,0}][{4,5}].wall_north() = W;
    w[{1,1,0}][{5,4}].wall_west() = W;
    return w;
}

void test1(world& w, StringView instance_name, const Function& make_dt, double accel)
{
    bool ret = run(w, make_dt,
                   Start{
                       .name = "test1"_s,
                       .instance = instance_name,
                       .pt = {{0,0,0}, {8,15}, {-8,  8}},
                       .accel = accel,
                       .rotation = N,
                   },
                   Expected{
                       .pt = {{0,0,0}, {8, 9}, {-6,-15}}, // distance_L1 == 3
                       .time = 6950*Millisecond,
                   },
                   Grace{
                       .time = 300*Millisecond,
                   });
    //fm_assert(ret);
    (void)ret;
}

void test2(world& w, StringView instance_name, const Function& make_dt, double accel)
{
    bool ret = run(w, make_dt,
               Start{
                   .name = "test2"_s,
                   .instance = instance_name,
                   .pt = {{-1,-1,0}, {13,14}, {-15,-29}},
                   .accel = accel,
                   .rotation = SE,
               },
               Expected{
                   .pt = {{1,1,0}, {4, 4}, {8,8}},
                   .time = 35'100*Millisecond,
               },
               Grace{
                   .time = 500*Millisecond,
                   .distance_L1 = 8,
               });
    //fm_assert(ret);
    (void)ret;
}

void test_critter(world& w1, world& w2)
{
    test1(w1, "dt=16.667 accel=1",   constantly(Millisecond * 16.667),    1);
    test1(w1, "dt=16.667 accel=2",   constantly(Millisecond * 16.667),    2);
    test1(w1, "dt=16.667 accel=5",   constantly(Millisecond * 16.667),    5);
    test1(w1, "dt=16.667 accel=0.5", constantly(Millisecond * 16.667),  0.5);
    test1(w1, "dt=33.334 accel=1",   constantly(Millisecond * 33.334),    1);
    test1(w1, "dt=33.334 accel=2",   constantly(Millisecond * 33.334),    2);
    test1(w1, "dt=33.334 accel=5",   constantly(Millisecond * 33.334),    5);
    test1(w1, "dt=33.334 accel=10",  constantly(Millisecond * 33.334),   10);
    test1(w1, "dt=50.000 accel=1",   constantly(Millisecond * 50.000),    1);
    test1(w1, "dt=50.000 accel=2",   constantly(Millisecond * 50.000),    2);
    test1(w1, "dt=50.000 accel=5",   constantly(Millisecond * 50.000),    5);
    test1(w1, "dt=100.00 accel=1",   constantly(Millisecond * 100.00),    1);
    test1(w1, "dt=100.00 accel=2",   constantly(Millisecond * 100.00),    2);
    test1(w1, "dt=100.00 accel=0.5", constantly(Millisecond * 100.00),  0.5);
    test1(w1, "dt=200.00 accel=1",   constantly(Millisecond * 200.00),    1);
    test1(w1, "dt=1.0000 accel=1",   constantly(Millisecond * 1.0000),    1);
    test1(w1, "dt=1.0000 accel=0.5", constantly(Millisecond * 1.0000),  0.5);

    test2(w2, "dt=16.667 accel=1",   constantly(Millisecond * 16.667),    1);
    test2(w2, "dt=16.667 accel=2",   constantly(Millisecond * 16.667),    2);
    test2(w2, "dt=16.667 accel=5",   constantly(Millisecond * 16.667),    5);
    test2(w2, "dt=16.667 accel=0.5", constantly(Millisecond * 16.667),  0.5);
    test2(w2, "dt=33.334 accel=1",   constantly(Millisecond * 33.334),    1);
    test2(w2, "dt=33.334 accel=2",   constantly(Millisecond * 33.334),    2);
    test2(w2, "dt=33.334 accel=5",   constantly(Millisecond * 33.334),    5);
    test2(w2, "dt=33.334 accel=10",  constantly(Millisecond * 33.334),   10);
    test2(w2, "dt=50.000 accel=1",   constantly(Millisecond * 50.000),    1);
    test2(w2, "dt=50.000 accel=2",   constantly(Millisecond * 50.000),    2);
    test2(w2, "dt=50.000 accel=5",   constantly(Millisecond * 50.000),    5);
    test2(w2, "dt=100.00 accel=1",   constantly(Millisecond * 100.00),    1);
    test2(w2, "dt=100.00 accel=2",   constantly(Millisecond * 100.00),    2);
    test2(w2, "dt=100.00 accel=0.5", constantly(Millisecond * 100.00),  0.5);
    test2(w2, "dt=200.00 accel=1",   constantly(Millisecond * 200.00),    1);
    test2(w2, "dt=1.0000 accel=1",   constantly(Millisecond * 1.0000),    1);
    test2(w2, "dt=1.0000 accel=0.5", constantly(Millisecond * 1.0000),  0.5);
}

void Critter_move(benchmark::State& st)
{
    auto w1 = make_world1(), w2 = make_world2();
    for (int i = 0; i < 3; i++)
        test_critter(w1, w2);
    for (auto _ : st)
        test_critter(w1, w2);
}

BENCHMARK(Critter_move)->Unit(benchmark::kMicrosecond);

world make_walk_world(bool wall)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    auto& c = w[{0,0,0}];
    if (wall)
        for (u8 x = 4; x <= 12; x++)
            c[{x, 9}].wall_north() = W;
    mark_all_modified(w);
    return w;
}

void Critter_walk(benchmark::State& st)
{
    constexpr u32 max_updates = 4000, stop_updates = 30;
    const bool path = st.range(0), diag = st.range(1);
    const auto dt = Second / (u64)st.range(3);
    // NW meets the wall mid-span. update_movement() slides W along it, move_toward() stops.
    const auto r = diag ? NW : N;
    const auto start = diag ? point{{0,0,0}, {15,15}, {-8, 8}} : point{{0,0,0}, {8,15}, {-8, 8}};
    const auto dest = diag ? point{{0,0,0}, {5,5}, {-8, 8}} : point{{0,0,0}, {8,3}, {-8, 8}};
    const auto dist = point::distance_l1(start, dest);

    auto w = make_walk_world(st.range(4));
    object_id id = 0;
    auto npc = w.ensure_player_character(id, make_proto((float)st.range(2)));
    u32 updates = 0;

    const auto walk = [&] {
        auto index = npc->index();
        npc->teleport_to(index, start, r);
        npc->delta = 0;
        auto last = npc->position();
        u32 stopped = 0;
        for (updates = 1; updates < max_updates; updates++)
        {
            if (path)
            {
                auto t = dt;
                if (npc->move_toward(index, t, dest).blocked || npc->position() == dest)
                    break;
                continue;
            }
            npc->update_movement(index, dt, r);
            const auto pos = npc->position();
            stopped = pos == last ? stopped + 1 : 0;
            last = pos;
            if (stopped == stop_updates || point::distance_l1(start, pos) >= dist)
                break;
        }
    };

    for (int i = 0; i < 3; i++)
        walk();
    for (auto _ : st)
        walk();
    st.counters["updates"] = updates;
    st.counters["walked"] = point::distance_l1(start, npc->position());

    // as run(): kill_object() would tear down a script this world never initialized
    auto& c = npc->chunk();
    c.sort_objects();
    c.remove_object(*npc, npc->index());
    npc.destroy();
}

BENCHMARK(Critter_walk)
    ->ArgNames({"path", "diag", "speed", "hz", "wall"})
    ->ArgsProduct({{0, 1}, {0, 1}, {1, 10, 50}, {60, 10}, {1, 0}})
    ->Unit(benchmark::kMicrosecond);

void Critter_walk_5x5(benchmark::State& st)
{
    constexpr u32 max_updates = 4000;
    const auto dt = Second / 60;
    const auto start = point{{0,0,0}, {8,15}, {-8, 8}}, dest = point{{0,0,0}, {8,3}, {-8, 8}};

    auto w = world();
    for (i16 y = -2; y <= 2; y++)
        for (i16 x = -2; x <= 2; x++)
            (void)w[{x, y, 0}];
    mark_all_modified(w);

    // Only the rays read a grid, from ray_pool, and with objects:1 from object_pool too.
    Pass::Pool walk_pool{Pass::Params{16, pass_mode::pass}};
    Pass::Pool shoot_pool{Pass::Params{16, pass_mode::shoot_through}};
    Grid::Cover::Pool cover_pool{Grid::Cover::Params{16}, shoot_pool};
    Grid::GridRegistry reg;
    constexpr auto ray_params = Pass::Params{16, pass_mode::shoot_through};
    auto& ray_pool = reg.pass_pool(ray_params);
    auto& object_pool = reg.object_pool(ray_params);
    const bool rays = st.range(1), objects = st.range(2);
    if (st.range(0))
        for (auto& c : w.chunks())
        {
            walk_pool[c].build_if_stale();
            cover_pool[c].build_if_stale();
            ray_pool[c].build_if_stale();
            if (objects)
                object_pool[c].build_if_stale();
        }

    object_id id = 0;
    auto npc = w.ensure_player_character(id, make_proto(10));
    const auto target = point{{1,0,0}, {8,8}, {}};
    u32 updates = 0;

    const auto walk = [&] {
        auto index = npc->index();
        npc->teleport_to(index, start, N);
        npc->delta = 0;
        for (updates = 1; updates < max_updates; updates++)
        {
            auto t = dt;
            if (npc->move_toward(index, t, dest).blocked || npc->position() == dest)
                break;
            if (rays)
            {
                const auto from = npc->position();
                auto res = objects ? raycast(w, from, target, npc->id, object_pool, Search::without_critters)
                                   : raycast(w, from, target, npc->id, ray_pool, Search::never_continue);
                benchmark::DoNotOptimize(res);
            }
        }
    };

    for (int i = 0; i < 3; i++)
        walk();
    for (auto _ : st)
        walk();
    st.counters["updates"] = updates;

    auto& c = npc->chunk();
    c.sort_objects();
    c.remove_object(*npc, npc->index());
    npc.destroy();
}

BENCHMARK(Critter_walk_5x5)
    ->ArgNames({"grids", "rays", "objects"})
    ->Args({0, 0, 0})->Args({1, 0, 0})->Args({1, 1, 0})->Args({1, 0, 1})->Args({1, 1, 1})
    ->Unit(benchmark::kMicrosecond);

// One npc runs the cover script for 600 updates while the threat circles its start 256 px away. Each
// iteration gets a fresh astar, so it fills the octants of a new encounter again.
void Critter_cover(benchmark::State& st)
{
    constexpr u32 updates = 600, div_size = 8;
    constexpr float radius = 256;
    constexpr auto body = Vector2ui{24, 24};
    constexpr auto center = Vector2i{352, 416};
    constexpr auto at = [](Vector2i xy) { return point{Vector3i{xy, 0}}; };

    auto w = world();
    make_cover_world(w);
    const auto add_critter = [&](point pos) {
        critter_proto p;
        p.name = "npc"_s;
        p.bbox_size = Vector2ub(body);
        p.offset = pos.offset();
        return w.make_object<critter>(w.make_id(), pos.coord(), move(p));
    };
    const auto circle = [&](u32 k) {
        const auto a = Rad{Deg{360.f * (float)k / (float)updates}};
        return at(center + Vector2i(Math::round(Vector2{Math::cos(a), Math::sin(a)} * radius)));
    };

    const auto npc_start = at(center);
    auto npc = add_critter(npc_start);
    auto threat = add_critter(circle(0));
    const auto skip_threat = Search::blocks_all_but(threat->id);
    // The threat stays where it is when its box at pos would overlap a collider.
    const auto move_threat = [&](point pos) {
        const auto min = Vector2(pos.intra().center_shifted() - Vector2i(body/2u));
        if (!Search::is_passable(w, pos.chunk3(), Range2D::fromSize(min, Vector2(body)), skip_threat))
            return false;
        auto i = threat->index();
        threat->teleport_to(i, pos, N);
        return true;
    };
    u32 k0 = 0;
    while (!move_threat(circle(k0)))
        fm_assert(++k0 < updates);
    const auto threat_start = threat->position();

    w.init_scripts();
    const auto frame = Second / npc->atlas->info().fps;
    Optional<astar> A;

    const auto reset = [&] {
        if (A)
            npc->script.do_clear(npc);
        auto i = npc->index();
        npc->teleport_to(i, npc_start, N);
        npc->delta = 0;
        auto j = threat->index();
        threat->teleport_to(j, threat_start, N);
        A.emplace();
        npc->script.do_reassign(critter_script::make_cover_script(*A, threat->id, div_size), npc);
    };
    const auto run = [&] {
        for (u32 k = 1; k <= updates; k++)
        {
            (void)move_threat(circle(k0 + k));
            w.increment_frame_no();
            w.advance_game_time(frame);
            auto i = npc->index();
            npc->update(npc, i, frame);
        }
    };

    reset();
    run();
    for (auto _ : st)
    {
        st.PauseTiming();
        reset();
        st.ResumeTiming();
        run();
    }

    const auto s = Cover::Script::get_status(*npc);
    fm_assert(s);
    const auto& n = s->counters;
    st.counters["num_searches"] = n.num_searches;
    st.counters["num_find_cover_point_calls"] = n.num_find_cover_point_calls;
    st.counters["num_rays"] = n.num_rays;
    st.counters["num_body_checks"] = n.num_body_checks;
    st.counters["num_dijkstra_calls"] = n.num_dijkstra_calls;
    constexpr u32 sweep_mask = 0x11111111u;
    u32 rays = 0, sweeps = 0;
    auto& pool = A->pool_registry().cover_pool(Grid::Cover::Params{div_size});
    for_cover_chunks([&](chunk_coords_ ch) {
        const auto k = pool[w[ch]].built_octants();
        rays += (u32)std::popcount(k & ~sweep_mask);
        sweeps += (u32)std::popcount(k & sweep_mask);
    });
    st.counters["ray_fills"] = rays;
    st.counters["sweep_fills"] = sweeps;
    st.counters["octant_fill_ms"] = (double)pool.octant_fill_time().stamp * 1e-6;

    npc->script.do_clear(npc);
    w.finish_scripts();
}

BENCHMARK(Critter_cover)->Unit(benchmark::kMillisecond);

} // namespace

} // namespace floormat
