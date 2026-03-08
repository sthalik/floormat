#include "app.hpp"
#include "run.hpp"
#include "src/nanosecond.inl"
#include "src/world.hpp"
#include "src/scenery-proto.hpp"
#include "src/tile-image.hpp"
#include "compat/function2.hpp"
#include "loader/loader.hpp"
#include <cinttypes>
#include <cstdio>

namespace floormat::Run {

namespace {

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
        auto npc = w.ensure_player_character(id, make_proto((float)accel));
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

} // namespace floormat::Run

namespace floormat {

void Test::test_critter()
{
    using namespace floormat::Run;

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
