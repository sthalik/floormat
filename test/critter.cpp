#include "app.hpp"
#include <cinttypes>
#include "compat/debug.hpp"
#include "compat/shared-ptr-wrapper.hpp"
#include "src/critter.hpp"
#include "src/world.hpp"
#include "src/wall-atlas.hpp"
#include "src/timer.hpp"
#include "src/log.hpp"
#include "loader/loader.hpp"
#include <iostream>

namespace floormat {

namespace {

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

template<typename F>
void run(StringView name, const F& make_dt, critter& npc,
         const point start, const rotation r,
         const point end, const Ns expected_time,
         const uint32_t fuzz_pixels, const Ns fuzz_time,
         const Ns max_time = Second*300, bool no_crash = false)
{
    constexpr uint32_t max_steps = 10'000;
    fm_assert(max_time <= Second*300);

    auto index = npc.index();
    npc.teleport_to(index, start, rotation_COUNT);

    char buf[81];
    Ns time{0};

    Debug{} << name << npc.position();

    uint32_t i;
    bool stopped = false;

    constexpr auto bar = Second*1000;

    for (i = 0; i <= max_steps; i++)
    {
        auto dt = Ns{make_dt()};
        //fm_assert(dt == Millisecond * 100);
        const auto last_pos = npc.position();
        Debug{} << "-" << last_pos << colon(',')
                << "time" << time
                << Debug::nospace << ", dt" << dt;
        fm_assert(dt >= Millisecond*1e-1);
        fm_assert(dt <= Second * 1000);
        npc.update_movement(index, dt, r);
        const auto pos = npc.position();
        time += dt;
        if (pos == last_pos)
        {
            stopped = true;
            Debug{} << "-" << last_pos << colon(',')
                    << "time" << time << Debug::nospace
                    << ", dt" << dt;
            Debug{} << "break!";
            break;
        }
        if (time > max_time) [[unlikely]]
        {
            Error{&std::cerr} << "timeout:" << max_time << "reached!";
            if (no_crash)
                return;
            else
                fm_EMIT_ABORT();
        }

        fm_assert(i != max_steps);
    }

    if (stopped)
    {
        Debug{} << "===>" << i << "iters" << colon(',')  << time;
        // todo! calc distance
    }
    else
        Debug{} << "!!! error: position doesn't converge after" << i << "iterations!";
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

using enum rotation;

template<typename F> void test1(const F& make_dt, int accel)
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

    run("test1"_s, make_dt, *player, init, N, end, Second*7, 16, Second*60);
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
    test1(constantly(Millisecond * 100), 1);
    test1(constantly(Millisecond * 10), 1);
    test1(constantly(Millisecond * 1), 1);
    Debug{} << "";
}

} // namespace floormat
