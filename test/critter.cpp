#include "app.hpp"
#include "compat/debug.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "compat/function2.hpp"
#include "src/critter.hpp"
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
#if 1
    bool quiet = !is_log_verbose();
    bool verbose = false;
#elif 1
    bool verbose = true;
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
    bool no_crash = false;
};

bool run(world& w, const function_view<Ns() const>& make_dt,
         Start start, Expected expected, Grace grace = {})
{
    constexpr auto max_time = 300*Second;
    constexpr uint32_t max_steps = 50'000;

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
    constexpr auto max_stop_frames = 20;
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
                    << " frac:" << npc.offset_frac;
    };

    auto fail = [b = grace.no_crash](const char* file, int line) {
        if (b) [[likely]]
            return false;
        else
        {
            fm_assert(false);
            fm_EMIT_DEBUG("", "assertion failed: false in %s:%d", file, line);
            fm_EMIT_ABORT();
        }
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
            if (frames_stopped == 0)
                saved_time = time;
            else if (frames_stopped >= max_stop_frames) [[unlikely]]
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
            Error{standard_error()} << "!!! fatal: timeout" << max_time << "reached!";
            return fail(__FILE__, __LINE__);
        }
        if (i > max_steps) [[unlikely]]
        {
            print_pos("*", start.pt, last_pos, time, dt, npc);
            Error{standard_error()} << "!!! fatal: position doesn't converge after" << i << "iterations!";
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
            Error{ standard_error(), Debug::Flag::NoSpace }
                << "!!! fatal: wrong time " << time
                << " expected:" << expected.time
                << " diff:" << grace.time
                << " for " << start.name << "/" << start.instance;
            return fail(__FILE__, __LINE__);
        }
    }

    return true;
}

void test1(StringView instance_name, const Function& make_dt, double accel)
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
                   });
    fm_assert(ret);
}

void test2(StringView instance_name, const Function& make_dt, double accel)
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
                   .time = 35'000*Millisecond,
               },
               Grace{
                   .time = 250*Millisecond,
                   .distance_L2 = 8,
               });
    fm_assert(ret);
}

} // namespace

void test_app::test_critter()
{
    // todo! add ANSI sequence to stdout to goto start of line and clear to eol
    // \r
    // <ESC>[2K
    // \n

    const bool is_noisy = !Start{}.quiet;
    if (is_noisy)
        DBG_nospace << "";

    test1("dt=16.667 accel=1",   constantly(Millisecond * 16.667),    1);
    test1("dt=16.667 accel=2",   constantly(Millisecond * 16.667),    2);
    test1("dt=16.667 accel=5",   constantly(Millisecond * 16.667),    5);
    test1("dt=33.337 accel=1",   constantly(Millisecond * 33.337),    1);
    test1("dt=33.337 accel=2",   constantly(Millisecond * 33.337),    2);
    test1("dt=33.337 accel=5",   constantly(Millisecond * 33.337),    5);
    test1("dt=16.667 accel=1",   constantly(Millisecond * 50.000),    1);
    test1("dt=16.667 accel=2",   constantly(Millisecond * 50.000),    2);
    test1("dt=16.667 accel=5",   constantly(Millisecond * 50.000),    5);
    test1("dt=200 accel=1",      constantly(Millisecond * 200.0 ),    1);
    test1("dt=100 accel=2",      constantly(Millisecond * 100.0 ),    2);
    test1("dt=16.667 accel=0.5", constantly(Millisecond * 16.667),  0.5);
    test1("dt=100 accel=0.5",    constantly(Millisecond * 100.00 ), 0.5);
    test1("dt=16.667 accel=1",   constantly(Millisecond * 16.667),    1);
    test2("dt=33.334 accel=1",   constantly(Millisecond * 33.334),    1);
    test2("dt=33.334 accel=2",   constantly(Millisecond * 33.334),    2);
    test2("dt=33.334 accel=5",   constantly(Millisecond * 33.334),    5);
    test2("dt=33.334 accel=10",  constantly(Millisecond * 33.334),   10);
    test2("dt=50.000 accel=1",   constantly(Millisecond * 50.000),    1);
    test2("dt=50.000 accel=2",   constantly(Millisecond * 50.000),    2);
    test2("dt=100.00 accel=1",   constantly(Millisecond * 100.00),    1);
    test2("dt=100.00 accel=2",   constantly(Millisecond * 100.00),    2);
    test2("dt=100.00 accel=0.5", constantly(Millisecond * 100.00),  0.5);

    if (is_noisy)
    {
        std::fputc('\t', stdout);
        std::fflush(stdout);
    }
}

} // namespace floormat
