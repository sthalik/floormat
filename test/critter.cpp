#include "app.hpp"
#include "compat/debug.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "compat/function2.hpp"
#include "src/critter.hpp"
#include "src/scenery-proto.hpp"
#include "src/world.hpp"
#include "src/wall-atlas.hpp"
#include "src/nanosecond.inl"
#include "src/log.hpp"
#include "src/point.inl"
#include "loader/loader.hpp"

#include <cinttypes>
#include <cstdio>

namespace floormat {

namespace {

using enum rotation;
using fu2::function_view;
using Function = function_view<Ns() const>;

#ifndef __CLION_IDE__
constexpr auto constantly(const auto& x) noexcept {
    return [x]<typename... Ts> (const Ts&...) constexpr -> const auto& { return x; };
}
#else
constexpr auto constantly(Ns x) noexcept { return [x] { return x; }; }
#endif

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
    for (auto& [coord, ch] : w.chunks())
        ch.mark_modified();
}

struct Start
{
    StringView name, instance;
    point pt;
    double accel = 1;
    enum rotation rotation = N;
    bool silent = false;
#if 1
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
    uint32_t distance_L2 = 24;
    uint32_t max_steps = 1'000;
    bool no_crash = false;

    static constexpr uint32_t very_slow_max_steps = 120'000,
                              slow_max_steps      = 12'000,
                              default_max_steps   = 1'200;
};

bool run(world& w, const function_view<Ns() const>& make_dt,
         Start start, Expected expected, Grace grace = {})
{
    start.quiet |= start.silent;
    start.verbose &= !start.silent;

    constexpr auto max_time = 300*Second;

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
    expected.time.stamp = uint64_t(expected.time.stamp / start.accel);
    fm_assert(expected.time <= max_time);
    if (grace.distance_L2 != (uint32_t)-1) [[unlikely]]
        fm_assert(grace.distance_L2 <= (uint32_t)Vector2((iTILE_SIZE2 * TILE_MAX_DIM)).length());

    mark_all_modified(w);

    object_id id = 0;
    auto npc_ = w.ensure_player_character(id, make_proto((float)start.accel)).ptr;
    auto& npc = *npc_;

    auto index = npc.index();
    npc.teleport_to(index, start.pt, rotation_COUNT);

    Ns time{0}, saved_time{0};
    auto last_pos = npc.position();
    uint32_t i;
    constexpr auto max_stop_frames = 250; // todo detect collisions properly and don't rely on this
    uint32_t frames_stopped = 0;

    if (!start.quiet) [[unlikely]]
        Debug{} << "**" << start.name << start.instance << colon();

    constexpr auto print_pos = [](StringView prefix, point start, point pos, Ns time, Ns dt, const critter& npc) {
        DBG_nospace << prefix
                    << " " << pos
                    << " time:" << time
                    << " dt:" << dt
                    << " dist:" << point::distance_l2(pos, start)
                    << " delta:" << npc.delta
                    << " frac:" << npc.offset_frac_;
    };

    auto fail = [b = grace.no_crash](const char* file, int line) {
        if (b) [[likely]]
            return false;
        else
            fm_emit_assert_fail("false", file, line);
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
                        << " distance:" << point::distance_l2(last_pos, expected.pt) << " px"
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
            if (!start.silent) [[unlikely]]
                Error{standard_error()} << "!!! fatal: timeout" << max_time << "reached!";
            return fail(__FILE__, __LINE__);
        }
        if (i >= grace.max_steps) [[unlikely]]
        {
            if (!start.quiet) [[unlikely]]
                print_pos("*", start.pt, last_pos, time, dt, npc);
            if (!start.silent) [[unlikely]]
                Error{ standard_error() } << "!!! fatal: position doesn't converge after" << i << "iterations!";
            return fail(__FILE__, __LINE__);
        }
    }

    if (const auto dist_l2 = point::distance_l2(last_pos, expected.pt);
        dist_l2 > grace.distance_L2) [[unlikely]]
    {
        Error{standard_error()} << "!!! fatal: distance" << dist_l2 << "pixels" << "over grace distance of" <<  grace.distance_L2;
        return fail(__FILE__, __LINE__);
    }
    else if (start.verbose) [[unlikely]]
        Debug{} << "*" << "distance:" << dist_l2 << "pixels";

    if (expected.time != Ns{}) [[likely]]
    {
        const auto time_diff = Ns{Math::abs((int64_t)expected.time.stamp - (int64_t)saved_time.stamp)};
        if (time_diff > grace.time)
        {
            if (!start.silent) [[unlikely]]
                Error{ standard_error(), Debug::Flag::NoSpace }
                    << "!!! fatal: wrong time " << saved_time
                    << " expected:" << expected.time
                    << " diff:" << time_diff
                    << " for " << start.name << "/" << start.instance;
            return fail(__FILE__, __LINE__);
        }
    }

    return true;
}

void test1(StringView instance_name, const Function& make_dt, double accel, uint32_t max_steps)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{{0,0,0}, {8,9}}].t.wall_north() = W;
    w[{{0,1,0}, {8,0}}].t.wall_north() = W;

    bool ret = run(w, make_dt,
                   Start{
                       .name = "test1"_s,
                       .instance = instance_name,
                       .pt = {{0,0,0}, {8,15}, {-8,  8}},
                       .accel = accel,
                       .rotation = N,
                   },
                   Expected{
                       .pt = {{0,0,0}, {8, 9}, {-6,-15}}, // distance_L2 == 3
                       .time = 6950*Millisecond,
                   },
                   Grace{
                       .time = 300*Millisecond,
                       .distance_L2 = 4,
                       .max_steps = max_steps,
                   });
    fm_assert(ret);
}

void test2(StringView instance_name, const Function& make_dt, double accel, uint32_t max_steps)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{{-1,-1,0}, {13,13}}].t.wall_north() = W;
    w[{{-1,-1,0}, {13,13}}].t.wall_west() = W;
    w[{{1,1,0}, {4,5}}].t.wall_north() = W;
    w[{{1,1,0}, {5,4}}].t.wall_west() = W;

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
                   .distance_L2 = 4,
                   .max_steps = max_steps,
               });
    fm_assert(ret);
}

void test3(StringView instance_name, const Function& make_dt, double accel, rotation r, bool no_unroll)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    auto S = loader.scenery("table0");

    auto w = world();
    w[{{-1,-1,0}, {13,13}}].t.wall_north() = W;
    w[{{-1,-1,0}, {13,13}}].t.wall_west() = W;
    w[{{1,1,0}, {4,5}}].t.wall_north() = W;
    w[{{1,1,0}, {5,4}}].t.wall_west() = W;
    w.make_scenery(w.make_id(), {{}, {0, 0}}, scenery_proto(S));
    w.make_scenery(w.make_id(), {{}, {1, 1}}, scenery_proto(S));
    w.make_scenery(w.make_id(), {{}, {14, 14}}, scenery_proto(S));
    w.make_scenery(w.make_id(), {{}, {15, 15}}, scenery_proto(S));
    w[chunk_coords_{}].sort_objects();

    if (no_unroll)
    {
        // reproduce the bug from commit 2b5a6e3f
        object_id id = 0;
        auto npc = w.ensure_player_character(id, make_proto((float)accel)).ptr;
        npc->set_bbox({}, {}, {1,1}, pass_mode::blocked);
    }

    bool ret = run(w, make_dt,
           Start{
               .name = "test3"_s,
               .instance = instance_name,
               .pt = {{0,0,0}, {8,8}, {}},
               .accel = accel,
               .rotation = r,
               .silent = true,
           },
           Expected{
               .pt = {},
               .time = Ns{},
           },
           Grace{
               //.time = 15*Second,
               //.distance_L2 = (uint32_t)-1,
               .max_steps = 1000,
               .no_crash = true,
           });
    fm_assert(!ret); // tiemout 300s reached
}

} // namespace

void test_app::test_critter()
{
    const bool is_noisy = !Start{}.quiet;
    if (is_noisy)
        DBG_nospace << "";

    test1("dt=16.667 accel=1",   constantly(Millisecond * 16.667),    1, Grace::default_max_steps);
    test1("dt=16.667 accel=2",   constantly(Millisecond * 16.667),    2, Grace::default_max_steps);
    test1("dt=16.667 accel=5",   constantly(Millisecond * 16.667),    5, Grace::default_max_steps);
    test1("dt=16.667 accel=0.5", constantly(Millisecond * 16.667),  0.5, Grace::default_max_steps*2);
    test1("dt=33.334 accel=1",   constantly(Millisecond * 33.334),    1, Grace::default_max_steps);
    test1("dt=33.334 accel=2",   constantly(Millisecond * 33.334),    2, Grace::default_max_steps);
    test1("dt=33.334 accel=5",   constantly(Millisecond * 33.334),    5, Grace::default_max_steps);
    test1("dt=33.334 accel=10",  constantly(Millisecond * 33.334),   10, Grace::default_max_steps);
    test1("dt=50.000 accel=1",   constantly(Millisecond * 50.000),    1, Grace::default_max_steps);
    test1("dt=50.000 accel=2",   constantly(Millisecond * 50.000),    2, Grace::default_max_steps);
    test1("dt=50.000 accel=5",   constantly(Millisecond * 50.000),    5, Grace::default_max_steps);
    test1("dt=100.00 accel=1",   constantly(Millisecond * 100.00),    1, Grace::default_max_steps);
    test1("dt=100.00 accel=2",   constantly(Millisecond * 100.00),    2, Grace::default_max_steps);
    test1("dt=100.00 accel=0.5", constantly(Millisecond * 100.00),  0.5, Grace::default_max_steps);
    test1("dt=200.00 accel=1",   constantly(Millisecond * 200.00),    1, Grace::default_max_steps);
    test1("dt=1.0000 accel=1",   constantly(Millisecond * 1.0000),    1, Grace::very_slow_max_steps);
    test1("dt=1.0000 accel=0.5", constantly(Millisecond * 1.0000),  0.5, Grace::very_slow_max_steps);

    test2("dt=16.667 accel=1",   constantly(Millisecond * 16.667),    1, Grace::default_max_steps*5);
    test2("dt=16.667 accel=2",   constantly(Millisecond * 16.667),    2, Grace::default_max_steps*3);
    test2("dt=16.667 accel=5",   constantly(Millisecond * 16.667),    5, Grace::default_max_steps);
    test2("dt=16.667 accel=0.5", constantly(Millisecond * 16.667),  0.5, Grace::slow_max_steps);
    test2("dt=33.334 accel=1",   constantly(Millisecond * 33.334),    1, Grace::default_max_steps*2);
    test2("dt=33.334 accel=2",   constantly(Millisecond * 33.334),    2, Grace::default_max_steps);
    test2("dt=33.334 accel=5",   constantly(Millisecond * 33.334),    5, Grace::default_max_steps);
    test2("dt=33.334 accel=10",  constantly(Millisecond * 33.334),   10, Grace::default_max_steps);
    test2("dt=50.000 accel=1",   constantly(Millisecond * 50.000),    1, Grace::default_max_steps);
    test2("dt=50.000 accel=2",   constantly(Millisecond * 50.000),    2, Grace::default_max_steps);
    test2("dt=50.000 accel=5",   constantly(Millisecond * 50.000),    5, Grace::default_max_steps);
    test2("dt=100.00 accel=1",   constantly(Millisecond * 100.00),    1, Grace::default_max_steps);
    test2("dt=100.00 accel=2",   constantly(Millisecond * 100.00),    2, Grace::default_max_steps);
    test2("dt=100.00 accel=0.5", constantly(Millisecond * 100.00),  0.5, Grace::default_max_steps);
    test2("dt=200.00 accel=1",   constantly(Millisecond * 200.00),    1, Grace::default_max_steps);
    test2("dt=1.0000 accel=1",   constantly(Millisecond * 1.0000),    1, Grace::very_slow_max_steps);
    test2("dt=1.0000 accel=0.5", constantly(Millisecond * 1.0000),  0.5, Grace::very_slow_max_steps);

    test3("dt=16.667 accel=50 r=E  no-unroll=false", constantly(Millisecond * 16.667), 50, E , false);
    test3("dt=16.667 accel=50 r=NE no-unroll=false", constantly(Millisecond * 16.667), 50, NE, false);
    test3("dt=16.667 accel=50 r=E  no-unroll=true",  constantly(Millisecond * 16.667), 50, E , true);
    test3("dt=16.667 accel=50 r=NE no-unroll=true",  constantly(Millisecond * 16.667), 50, NE, true);

    if (is_noisy)
    {
        std::fputc('\t', stdout);
        std::fflush(stdout);
    }
}

} // namespace floormat
