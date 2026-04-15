#include "run.hpp"
#include "compat/debug.hpp"
#include "compat/function2.hpp"
#include "src/point.inl"
#include "src/tile-constants.hpp"
#include "src/nanosecond.inl"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include <mg/Functions.h>

namespace floormat::Run {

namespace {

namespace fm_debug = floormat::debug::detail;

} // namespace

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

bool run(world& w, const function_view<Ns() const>& make_dt,
         Start start, Expected expected, Grace grace)
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
    auto npc_ = w.ensure_player_character(id, make_proto((float)start.accel));
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
                    << " frac:" << npc.offset_frac;
    };

    auto fail = [b = grace.no_crash](const char* file, int line) {
        if (b) [[likely]]
            return false;
        else
            fm_debug::emit_abort(file, line, "false");
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

} // namespace floormat::Run
