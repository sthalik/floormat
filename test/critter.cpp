#include "app.hpp"
#include <cinttypes>
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
#include <iostream>

// todo! find all places where singed division is used

namespace floormat {

namespace {

using enum rotation;
using fu2::function_view;

constexpr auto constantly(const auto& x) noexcept {
    return [x]<typename... Ts> (const Ts&...) constexpr -> const auto& { return x; };
}

critter_proto make_proto(int accel)
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

struct Start
{
    StringView name, instance;
    point pt;
    enum rotation rotation = N;
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

bool run(StringView subtest_name, critter& npc, const function_view<Ns() const>& make_dt,
         Start start, Expected expected, Grace grace = {})
{
    //validate_start(start);
    //validate_expected(expected);
    //validate_grace(grace);
    fm_assert(start.name);
    fm_assert(start.instance);
    fm_assert(start.rotation < rotation_COUNT);
    fm_assert(expected.time > Ns{});
    fm_assert(expected.time <= grace.max_time);
    fm_assert(grace.max_time <= 10*Minutes);
    fm_assert(grace.max_time > Ns{});
    fm_assert(grace.distance_L2 <= (uint32_t)Vector2((iTILE_SIZE2 * TILE_MAX_DIM)).length());
    fm_assert(grace.max_steps > 0);
    fm_assert(grace.max_steps <= 1000'00);

    auto index = npc.index();
    npc.teleport_to(index, start.pt, rotation_COUNT);

    char buf[81];
    Ns time{0};

    auto last_pos = npc.position();
    uint32_t i;
    bool stopped = false;

    if (subtest_name)
        Debug{} << "-----" << start.name << "->" << start.instance << Debug::nospace << subtest_name << "-----";
    else
        Debug{} << "-----" << start.name << subtest_name << "-----";

    constexpr auto print_pos = [](StringView prefix, point start, point pos, Ns time, Ns dt) {
        DBG_nospace << prefix << " " << pos << "--"
                    << " time:" << time
                    << " dt:" << dt
                    << " dist:" << point::distance_l2(pos, start);
    };

    for (i = 0; i <= grace.max_steps; i++)
    {
        const auto dt = Ns{make_dt()};
        if (dt == Ns{}) [[unlikely]]
        {
            Debug{} << "| dt == 0, breaking";
            break;
        }
        print_pos("-", expected.pt, npc.position(), time, dt);
        fm_assert(dt >= Millisecond*1e-1);
        fm_assert(dt <= Second * 1000);
        npc.update_movement(index, dt, start.rotation);
        const auto pos = npc.position();
        const bool same_pos = pos == last_pos;
        last_pos = pos;

        time += dt;

        if (same_pos) [[unlikely]]
        {
            print_pos("-",  expected.pt, pos, time, dt);
            Debug{} << "===>" << i << "iters" << colon(',')  << time;
            fm_assert(i != 0);
            break;
        }
        if (time > grace.max_time) [[unlikely]]
        {
            print_pos("*", start.pt, last_pos, time, dt);
            Error{&std::cerr} << "!!! fatal: timeout" << grace.max_time << "reached!";
            if (grace.no_crash)
                return false;
            else
                fm_assert(false);
        }
        if (i > grace.max_steps) [[unlikely]]
        {
            print_pos("*", start.pt, last_pos, time, dt);
            Error{&std::cerr} << "!!! fatal: position doesn't converge after" << i << "iterations!";
            if (grace.no_crash)
                return false;
            else
                fm_assert(false);
        }
    }



    if (const auto dist_l2 = point::distance_l2(last_pos, expected.pt);
        dist_l2 > grace.distance_L2)
    {
        Error{&std::cerr} << "!!! error: distance" << dist_l2 << "pixels" << "over grace distance of" <<  grace.distance_L2;
        if (grace.no_crash)
            fm_assert(false);
        else
            fm_assert(false);
    }
    else
        Debug{} << "*" << "distance:" << dist_l2 << "pixels";

    return true;
}

/* ***** TEST 1 *****
 *
 * wall n 0x0 - 8:9
 * wall n 0x1 - 8:0
 *
 * bbox-offset=0 bbox-size=32x32
 *
 * npc speed=1 ==> 6800 ms
 * npc speed=5 ==> 1350 ms
 *
 * before chunk=0x0 tile=8:15 offset=-8:8
 * after  chunk=0x0 tile=8:9  offset=-8:-16
 *
 * time=6800ms
*/

template<typename F> void test1(StringView instance_name, const F& make_dt, int accel)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{{0,0,0}, {8,9}}].t.wall_north() = W;
    w[{{0,1,0}, {8,0}}].t.wall_north() = W;

    constexpr point init {{0,0,0}, {8,15}, {-8,  8}};
    constexpr point end  {{0,0,0}, {8, 9}, {-8,-16}};

    object_id id = 0;
    auto player = w.ensure_player_character(id, make_proto(accel)).ptr;

    w[chunk_coords_{0,0,0}].mark_modified();
    w[chunk_coords_{0,1,0}].mark_modified();

    bool ret = run("test1", *player, make_dt,
                   Start{
                       .name = "test1",
                       .instance = instance_name,
                       .pt = init,
                       .rotation = N,
                   },
                   Expected{
                       .pt = end,
                       .time = Millisecond*6950,
                   },
                   Grace{});
    fm_assert(ret);

    //run("test1"_s, make_dt, *player, init, N, end, Second*7, Millisecond*16.667, Second*60);
}

/* ***** TEST 2 *****
 *
 */

template<typename F> void test2(F&& make_dt, int accel)
{
    // TODO diagonal!
}

} // namespace

void test_app::test_critter()
{
    Debug{} << "";
    Debug{} << "--";
    Debug{} << "";
    test1("dt=1000 ms",constantly(Millisecond * 1000), 1);
    test1("dt=100 ms", constantly(Millisecond * 100), 1);
    test1("dt=50 ms", constantly(Millisecond * 50), 1);
    test1("dt=16.667 ms", constantly(Millisecond * 16.667), 1);
    Debug{} << "";
}

} // namespace floormat
