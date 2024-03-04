#include "app.hpp"
#include "compat/debug.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "compat/function2.hpp"
#include "src/critter.hpp"
#include "src/world.hpp"
#include "src/wall-atlas.hpp"
#include "src/timer.hpp"
#include "src/log.hpp"
#include "src/point.inl"
#include "loader/loader.hpp"
#include <cinttypes>
#include <cstdio>
// todo! find all places where singed division is used

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
    float accel = 1;
    enum rotation rotation = N;
    bool verbose = is_log_verbose();
#if 0
    bool quiet = is_log_quiet();
#else
    bool quiet = !is_log_verbose();
#endif
};

struct Expected
{
    point pt;
    Ns time;
};

struct Grace
{
    Ns max_time = 600*Seconds;
    uint32_t distance_L2 = 24;
    uint32_t max_steps = 10'000;
    bool no_crash = false;
};

bool run(StringView subtest_name, world& w, const function_view<Ns() const>& make_dt,
         Start start, Expected expected, Grace grace = {})
{
    fm_assert(!start.quiet | !start.verbose);
    //validate_start(start);
    //validate_expected(expected);
    //validate_grace(grace);
    fm_assert(start.name);
    fm_assert(start.instance);
    fm_assert(start.rotation < rotation_COUNT);
    fm_assert(expected.time <= grace.max_time);
    fm_assert(grace.max_time <= 10*Minutes);
    fm_assert(grace.max_time > Ns{});
    fm_assert(grace.distance_L2 <= (uint32_t)Vector2((iTILE_SIZE2 * TILE_MAX_DIM)).length());
    fm_assert(grace.max_steps > 0);
    fm_assert(grace.max_steps <= 1000'00);

    mark_all_modified(w);

    object_id id = 0;
    auto npc_ = w.ensure_player_character(id, make_proto(start.accel)).ptr;
    auto& npc = *npc_;

    auto index = npc.index();
    npc.teleport_to(index, start.pt, rotation_COUNT);

    Ns time{0};
    auto last_pos = npc.position();
    uint32_t i;

    if (!start.quiet) [[unlikely]]
    {
        if (subtest_name)
            Debug{} << "**" << start.name << "->" << start.instance << Debug::nospace << subtest_name << colon();
        else
            Debug{} << "**" << start.name << subtest_name << colon();
    }

    constexpr auto print_pos = [](StringView prefix, point start, point pos, Ns time, Ns dt) {
        DBG_nospace << prefix
                    << " " << pos
                    << " time:" << time
                    << " dt:" << dt
                    << " dist:" << point::distance_l2(pos, start);
    };

    for (i = 0; i <= grace.max_steps; i++)
    {
        const auto dt = Ns{make_dt()};
        if (dt == Ns{}) [[unlikely]]
        {
            if (!start.quiet) [[unlikely]]
                Debug{} << "| dt == 0, breaking";
            break;
        }
        if (start.verbose) [[unlikely]]
            print_pos("  ", expected.pt, npc.position(), time, dt);
        fm_assert(dt >= Millisecond*1e-1);
        fm_assert(dt <= Second * 1000);
        npc.update_movement(index, dt, start.rotation);
        const auto pos = npc.position();
        const bool same_pos = pos == last_pos;
        last_pos = pos;

        time += dt;

        if (same_pos) [[unlikely]]
        {
            if (!start.quiet) [[unlikely]]
            {
                print_pos("->", expected.pt, pos, time, dt);
                DBG_nospace << "===>"
                            << " iters" << colon(',')
                            << " time" << time
                            << Debug::newline;
            }
            if (i == 0) [[unlikely]] // todo! check for very small dt before dying
            {
                { auto dbg = Error{standard_error(), Debug::Flag::NoSpace};
                  dbg << "!!! fatal: took zero iterations";
                  dbg << " dt=" << dt << " accel=" << npc.speed;
                }
                fm_assert(false);
            }
            break;
        }
        if (time > grace.max_time) [[unlikely]]
        {
            if (!start.quiet) [[unlikely]]
                print_pos("*", start.pt, last_pos, time, dt);
            Error{standard_error()} << "!!! fatal: timeout" << grace.max_time << "reached!";
            if (grace.no_crash)
                return false;
            else
                fm_assert(false);
        }
        if (i > grace.max_steps) [[unlikely]]
        {
            print_pos("*", start.pt, last_pos, time, dt);
            Error{standard_error()} << "!!! fatal: position doesn't converge after" << i << "iterations!";
            if (grace.no_crash)
                return false;
            else
                fm_assert(false);
        }
    }

    if (const auto dist_l2 = point::distance_l2(last_pos, expected.pt);
        dist_l2 > grace.distance_L2) [[unlikely]]
    {
        Error{standard_error()} << "!!! fatal: distance" << dist_l2 << "pixels" << "over grace distance of" <<  grace.distance_L2;
        if (grace.no_crash)
            return false;
        else
            fm_assert(false);
    }
    else if (start.verbose) [[unlikely]]
        Debug{} << "*" << "distance:" << dist_l2 << "pixels";

    if (expected.time != Ns{})
    {
        const auto time_diff = Ns{Math::abs((int64_t)expected.time.stamp - (int64_t)time.stamp)};
        if (time_diff > expected.time)
        {
            Error{standard_error()} << "!!! fatal: time" << time_diff << "over tolerance of" <<  grace.max_time;
            if (grace.no_crash)
                return false;
            else
                fm_assert(false);
        }
    }

    return true;
}

void test1(StringView instance_name, const Function& make_dt, float accel)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{{0,0,0}, {8,9}}].t.wall_north() = W;
    w[{{0,1,0}, {8,0}}].t.wall_north() = W;

    bool ret = run("test1", w, make_dt,
                   Start{
                       .name = "test1"_s,
                       .instance = instance_name,
                       .pt = {{0,0,0}, {8,15}, {-8,  8}},
                       .accel = accel,
                       .rotation = N,
                   },
                   Expected{
                       .pt = {{0,0,0}, {8, 9}, {-6,-15}}, // distance_L2 == 3
                       .time = Millisecond*6950,
                   },
                   Grace{});
    fm_assert(ret);
}

void test2(StringView instance_name, const Function& make_dt, float accel)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{{-1,-1,0}, {13,13}}].t.wall_north() = W;
    w[{{-1,-1,0}, {13,13}}].t.wall_west() = W;
    w[{{1,1,0}, {4,5}}].t.wall_north() = W;
    w[{{1,1,0}, {5,4}}].t.wall_west() = W;

    bool ret = run("test1", w, make_dt,
               Start{
                   .name = "test1"_s,
                   .instance = instance_name,
                   .pt = {{1,1,0}, {4,4}, {-29, 8}},
                   .accel = accel,
                   .rotation = NW,
               },
               Expected{
                   .pt = {{-1,-1,0}, {13, 13}, {-16,-16}}, // distance_L2 == 3
                   .time = accel == 1 ? Ns{Millisecond*6900} : Ns{},
               },
               Grace{});
    fm_assert(ret);
}

} // namespace

void test_app::test_critter()
{
    // todo! add ANSI sequence to stdout to goto start of line and clear to eol
    // \r
    // <ESC>[2K
    // \n

    if (!is_log_quiet())
        DBG_nospace << "";
    test1("dt=1000 accel=1",    constantly(Millisecond * 1000  ),  1);
    test1("dt=1000 accel=5",    constantly(Millisecond * 1000  ),  5);
    test1("dt=100 accel=5",     constantly(Millisecond * 100   ),  5);
    test1("dt=50 accel=5",      constantly(Millisecond * 50    ),  5);
    test1("dt=16.667 accel=10", constantly(Millisecond * 16.667), 10);
    test1("dt=16.667 accel=1",  constantly(Millisecond * 16.667),  1);
    //test1("dt=16.5 ms accel=1", constantly(Millisecond * 16.5),  1); // todo! fix this!
    test2("dt=16.667 accel=5",  constantly(Millisecond * 16.667),  5);
    if (!is_log_quiet())
    {
        std::fputc('\t', stdout);
        std::fflush(stdout);
    }
}

} // namespace floormat
