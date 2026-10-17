#include "app.hpp"
#include "run.hpp"
#include "src/nanosecond.inl"
#include "src/point.inl"
#include "src/world.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/anim-atlas.hpp"
#include "src/scenery-proto.hpp"
#include "src/tile-image.hpp"
#include "src/raycast-line.hpp"
#include "src/sweep-aabb.hpp"
#include "src/chunk.hpp"
#include "loader/scenery-cell.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include "loader/loader.hpp"
#include "compat/constantly.hpp"
#include <cr/GrowableArray.h>
#include <cinttypes>
#include <cstdio>

namespace floormat::Run {

namespace {

void test1(StringView instance_name, const Function& make_dt, double accel, uint32_t max_steps)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{0,0,0}][{8,9}].wall_north() = W;
    w[{0,1,0}][{8,0}].wall_north() = W;

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
                       .distance_L1 = 4,
                       .max_steps = max_steps,
                   });
    fm_assert(ret);
}

void test2(StringView instance_name, const Function& make_dt, double accel, uint32_t max_steps)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };

    auto w = world();
    w[{-1,-1,0}][{13,13}].wall_north() = W;
    w[{-1,-1,0}][{13,13}].wall_west() = W;
    w[{1,1,0}][{4,5}].wall_north() = W;
    w[{1,1,0}][{5,4}].wall_west() = W;

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
                   .distance_L1 = 4,
                   .max_steps = max_steps,
               });
    fm_assert(ret);
}

void test3(StringView instance_name, const Function& make_dt, double accel, rotation r, bool no_unroll)
{
    const auto W = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    auto S = loader.scenery("table0");

    auto w = world();
    w[{-1,-1,0}][{13,13}].wall_north() = W;
    w[{-1,-1,0}][{13,13}].wall_west() = W;
    w[{1,1,0}][{4,5}].wall_north() = W;
    w[{1,1,0}][{5,4}].wall_west() = W;
    w.make_scenery(w.make_id(), {{}, {0, 0}}, scenery_proto(S));
    w.make_scenery(w.make_id(), {{}, {1, 1}}, scenery_proto(S));
    w.make_scenery(w.make_id(), {{}, {14, 14}}, scenery_proto(S));
    w.make_scenery(w.make_id(), {{}, {15, 15}}, scenery_proto(S));
    w[{}].sort_objects();

    if (no_unroll)
    {
        // reproduce the bug from commit 2b5a6e3f
        object_id id = 0;
        auto npc = w.ensure_player_character(id, make_proto((float)accel));
        npc->set_bbox({}, {}, {4,4}, pass_mode::blocked);
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
               //.distance_L1 = (uint32_t)-1,
               .max_steps = 1000,
               .no_crash = true,
           });
    fm_assert(!ret); // tiemout 300s reached
}

point walk_for(Ns dt_per_step, Ns total)
{
    auto w = world();
    object_id id = 0;
    auto npc_ = w.ensure_player_character(id, make_proto(1.f));
    auto& npc = *npc_;
    auto idx = npc.index();
    npc.teleport_to(idx, {{0, 0, 0}, {8, 8}, {0, 0}}, rotation_COUNT);
    Ns elapsed{0};
    while (elapsed < total)
    {
        npc.update_movement(idx, dt_per_step, rotation::N);
        elapsed = elapsed + dt_per_step;
    }
    return npc.position();
}

void test_dt_invariance()
{
    constexpr Ns total = Second;
    const auto p_1ms   = walk_for(Millisecond * 1.0,    total);
    const auto p_60fps = walk_for(Millisecond * 16.667, total);
    const auto p_30fps = walk_for(Millisecond * 33.334, total);
    const auto p_10fps = walk_for(Millisecond * 100.0,  total);

    constexpr uint32_t tol = 2;
    fm_assert(point::distance_l1(p_1ms,   p_60fps) <= tol);
    fm_assert(point::distance_l1(p_30fps, p_60fps) <= tol);
    fm_assert(point::distance_l1(p_10fps, p_60fps) <= tol);
}

void test_offset_frac_diag_to_cardinal()
{
    auto w = world();
    object_id id = 0;
    auto npc_ = w.ensure_player_character(id, make_proto(1.f));
    auto& npc = *npc_;
    auto idx = npc.index();
    const auto frame = Second / npc.atlas->info().fps;

    // leaves a leftover over 1 px, which a cardinal step must not spend in one frame
    const auto walk_diagonal = [&] {
        npc.teleport_to(idx, {{0, 0, 0}, {8, 8}, {0, 0}}, rotation_COUNT);
        npc.offset_frac = {};
        for (auto i = 0u; i < 4; i++)
            npc.update_movement(idx, frame, rotation::SE);
        return npc.position();
    };

    const auto p0 = walk_diagonal();
    npc.update_movement(idx, frame, rotation::E);
    fm_assert_equal(Vector2i{1, 0}, npc.position() - p0);

    const auto p1 = walk_diagonal();
    auto dt = frame;
    (void)npc.move_toward(idx, dt, point::normalize_coords(p1, Vector2i{10, 0}));
    fm_assert_equal(Vector2i{1, 0}, npc.position() - p1);
}

struct line_walker
{
    world w;
    critter* npc;
    size_t idx;
    Ns frame;

    line_walker() : w{world()}
    {
        object_id id = 0;
        npc = &*w.ensure_player_character(id, make_proto(1.f));
        idx = npc->index();
        frame = Second / npc->atlas->info().fps;
    }

    void place(point pt)
    {
        npc->teleport_to(idx, pt, rotation_COUNT);
        npc->offset_frac = {};
        npc->delta = {};
    }

    critter::move_result step(point from, point dest, uint32_t nframes = 1)
    {
        dt = frame * nframes;
        return npc->move_toward(idx, dt, from, dest);
    }

    Ns dt;
};

void test_move_toward_line()
{
    line_walker L;
    struct segment { Vector2i vec; rotation r; };
    // 57/137 is just over tan 22.5°, 56/137 just under. Both cross into chunk (1, 0).
    for (auto [vec, r] : { segment{{137, 57}, rotation::SE}, segment{{137, 56}, rotation::E},
                           segment{{-23, -71}, rotation::N}, segment{{-50, 61}, rotation::SW} })
    {
        const auto from = point{{0, 0, 0}, {14, 14}, {3, -5}}, dest = from + vec;
        const auto line = rc::bresenham_line{{}, vec};
        L.place(from);
        uint32_t frames = 0, k = 0;
        while (L.npc->position() != dest)
        {
            fm_assert(frames++ < 1000);
            const auto res = L.step(from, dest);
            fm_assert(!res.blocked);
            const auto px = L.npc->position() - from;
            const auto k2 = line.step_of(px);
            fm_assert_equal(line.pixel_at(k2), px);
            fm_assert(k2 >= k);
            k = k2;
        }
        fm_assert(L.npc->r == r);
        const auto len = Vector2(vec).length();
        fm_assert(Math::abs(float(frames) - Math::ceil(len)) <= 1);
        fm_assert(!L.step(from, dest).moved);

        L.place(from);
        const auto res = L.step(from, dest, (uint32_t)Math::ceil(len) + 50);
        fm_assert(res.moved && !res.blocked && L.npc->position() == dest);
        fm_assert(L.dt == L.frame * 50u);
    }
}

void test_move_toward_line_blocked()
{
    line_walker L;
    constexpr auto ch = chunk_coords_{0, 0, 0};
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_offset = {};
    p.bbox_size   = Vector2ub{20, 20};
    p.pass        = pass_mode::blocked;
    L.w.make_scenery(L.w.make_id(), {ch, {5, 3}}, move(p));
    L.w[ch].mark_modified();

    const auto from = point{ch, {2, 2}, {}}, dest = from + Vector2i{300, 130};
    const auto origin = point{ch, {}, {}};
    const auto self_id = L.npc->id;
    const auto pred = [self_id](chunk&, collision_data x, Range2D) {
        return x.id == self_id ? path_search_continue::pass : path_search_continue::blocked;
    };
    const auto line = rc::bresenham_line{from - origin, dest - origin};
    const auto expected = find_swept_collider(L.w, ch, line, 0, line.steps(),
                                              Vector2ui(L.npc->bbox_size), pred);
    fm_assert(expected.has_collider && expected.step > 1);

    L.place(from);
    critter::move_result res{};
    for (auto i = 0u; i < 1000 && !res.blocked; i++)
        res = L.step(from, dest);
    fm_assert(res.blocked && !res.displaced);
    fm_assert_equal(line.pixel_at(expected.step - 1), L.npc->position() - origin);
    const auto stopped = L.npc->position();
    bool blocked = false;
    for (auto i = 0; i < 2; i++)
    {
        const auto again = L.step(from, dest);
        fm_assert(!again.moved);
        blocked |= again.blocked;
    }
    fm_assert(blocked && L.npc->position() == stopped);

    L.place(from);
    res = L.step(from, dest, 400);
    fm_assert(res.blocked && res.moved);
    fm_assert(L.npc->position() == stopped);
}

void test_move_toward_line_displaced()
{
    line_walker L;
    const auto from = point{{0, 0, 0}, {4, 4}, {}}, dest = from + Vector2i{137, 57};
    L.place(from);
    for (auto i = 0; i < 10; i++)
        (void)L.step(from, dest);
    // same x as a line pixel, one row below it
    const auto off = L.npc->position() + Vector2i{0, 1};
    L.npc->teleport_to(L.idx, off, rotation_COUNT);
    const auto res = L.step(from, dest);
    fm_assert(res.displaced && res.blocked && !res.moved);
    fm_assert(L.npc->position() == off);
}

struct script_walker
{
    astar A;
    world w;
    bptr<critter> npc;
    Ns frame;

    script_walker() : w{world()}
    {
        for (int16_t j = -1; j <= 1; j++)
            for (int16_t i = -1; i <= 1; i++)
                (void)w[chunk_coords_{i, j, 0}];
        object_id id = 0;
        npc = w.ensure_player_character(id, make_proto(1.f));
        frame = Second / npc->atlas->info().fps;
        w.init_scripts();
    }

    ~script_walker() { w.finish_scripts(); }

    void place(point pt)
    {
        auto i = npc->index();
        npc->teleport_to(i, pt, rotation_COUNT);
        npc->offset_frac = {};
        npc->delta = {};
    }

    path_search_result find_path(point from, point to, bool smooth)
    {
        const auto size = Vector2ui(npc->bbox_size);
        auto res = A.Dijkstra(w, from, to, 4096, size, Search::without_critters());
        fm_assert(res.is_found());
        if (smooth)
            fm_assert(A.string_pull(w, res, size, Search::without_critters()));
        return res;
    }

    void walk(path_search_result res) { npc->script.do_reassign(critter_script::make_walk_script(move(res)), npc); }
    void update(uint32_t nframes = 1) { auto i = npc->index(); npc->update(npc, i, frame * nframes); }

    uint32_t walk_to_end()
    {
        uint32_t frames = 0;
        while (npc->last_walk == critter::walk_result::none)
        {
            fm_assert(frames++ < 2000);
            update();
        }
        return frames;
    }
};

bool on_segment(point a, point b, point p)
{
    const auto origin = point{a.chunk3(), {}, {}};
    const auto line = rc::bresenham_line{a - origin, b - origin};
    for (auto k = 0u; k <= line.steps(); k++)
        if (line.pixel_at(k) == p - origin)
            return true;
    return false;
}

void test_walk_smoothed()
{
    constexpr auto ch = chunk_coords_{0, 0, 0};
    script_walker W;
    scenery_proto p;
    p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype   = generic_scenery_proto{};
    p.bbox_size = Vector2ub{40, 40};
    p.pass      = pass_mode::blocked;
    W.w.make_scenery(W.w.make_id(), {ch, {7, 6}}, move(p));
    W.w[ch].mark_modified();

    const auto from = point{ch, {2, 6}, {}}, to = point{ch, {12, 6}, {}};
    W.place(from);
    auto res = W.find_path(from, to, true);
    Array<point> sp;
    for (const auto& x : std::as_const(res).smoothings())
        arrayAppend(sp, x.pt);
    fm_assert(sp.size() == 4);
    float len = 0;
    for (auto j = 0uz; j + 1 < sp.size(); j++)
        len += Vector2(sp[j+1] - sp[j]).length();

    W.walk(move(res));
    uint32_t frames = 0, seg = 0;
    while (W.npc->last_walk == critter::walk_result::none)
    {
        fm_assert(frames++ < 2000);
        W.update();
        while (!on_segment(sp[seg], sp[seg+1], W.npc->position()))
        {
            seg++;
            fm_assert(seg + 1 < sp.size());
        }
    }
    fm_assert(W.npc->last_walk == critter::walk_result::arrived);
    fm_assert(W.npc->position() == to && !W.npc->moves.AUTO);
    fm_assert(float(frames) >= len - 1 && float(frames) <= Math::ceil(len) + float(sp.size()));

    W.place(from);
    W.walk(W.find_path(from, to, false));
    const auto raw_frames = W.walk_to_end();
    fm_assert(W.npc->last_walk == critter::walk_result::arrived && W.npc->position() == to);
    fm_assert(frames < raw_frames);

    W.place(from);
    W.walk(W.find_path(from, to, true));
    W.update(frames + 50);
    fm_assert(W.npc->last_walk == critter::walk_result::arrived && W.npc->position() == to);
}

// The path is found without critters, so one standing on the segment stops the walk.
void test_walk_smoothed_blocked()
{
    constexpr auto ch = chunk_coords_{0, 0, 0};
    script_walker W;
    const auto from = point{ch, {2, 3}, {}}, to = point{ch, {13, 8}, {5, -3}};
    W.place(from);
    auto res = W.find_path(from, to, true);
    fm_assert(std::as_const(res).smoothings().size() == 2);

    const auto mid = from + (to - from) / 2;
    auto proto = make_proto(1.f);
    proto.playable = false;
    proto.offset = mid.offset();
    const auto other = W.w.make_object<critter>(W.w.make_id(), mid.coord(), move(proto));

    const auto origin = point{ch, {}, {}};
    const auto line = rc::bresenham_line{from - origin, to - origin};
    const auto self_id = W.npc->id;
    const auto pred = [self_id](chunk&, collision_data x, Range2D) {
        return x.id == self_id ? path_search_continue::pass : path_search_continue::blocked;
    };
    const auto expected = find_swept_collider(W.w, ch, line, 0, line.steps(), Vector2ui(W.npc->bbox_size), pred);
    fm_assert(expected.has_collider && expected.collider.id == other->id && expected.step > 1);

    W.walk(move(res));
    (void)W.walk_to_end();
    fm_assert(W.npc->last_walk == critter::walk_result::blocked && !W.npc->moves.AUTO);
    fm_assert_equal(line.pixel_at(expected.step - 1), W.npc->position() - origin);
}

void test_walk_smoothed_displaced()
{
    constexpr auto ch = chunk_coords_{0, 0, 0};
    script_walker W;
    const auto from = point{ch, {2, 3}, {}}, to = point{ch, {13, 8}, {5, -3}};
    W.place(from);
    auto res = W.find_path(from, to, true);
    fm_assert(std::as_const(res).smoothings().size() == 2);
    W.walk(move(res));
    for (auto i = 0; i < 10; i++)
        W.update();
    fm_assert(W.npc->last_walk == critter::walk_result::none);

    // the line is x-major, so one row below its pixel is off it
    const auto off = W.npc->position() + Vector2i{0, 1};
    W.place(off);
    W.update();
    fm_assert(W.npc->last_walk == critter::walk_result::displaced && !W.npc->moves.AUTO);
    fm_assert(W.npc->position() == off);
}

void test_walk_interrupted()
{
    constexpr auto ch = chunk_coords_{0, 0, 0};
    script_walker W;
    const auto from = point{ch, {2, 3}, {}}, to = point{ch, {13, 8}, {5, -3}};
    W.place(from);
    W.npc->last_walk = critter::walk_result::blocked;
    W.walk(W.find_path(from, to, true));
    fm_assert(W.npc->last_walk == critter::walk_result::none);
    for (auto i = 0; i < 10; i++)
        W.update();
    W.npc->set_keys(true, false, false, false);
    W.update();
    fm_assert(!W.npc->moves.AUTO && W.npc->last_walk == critter::walk_result::none);
}

void test_player_character_id_lowest()
{
    auto w = world();
    auto p = world::make_player_proto();
    auto a = w.make_object<critter>(w.make_id(), {{0, 0, 0}, {1, 1}}, critter_proto{p});
    auto b = w.make_object<critter>(w.make_id(), {{0, 0, 0}, {2, 2}}, critter_proto{p});
    fm_assert(a->id < b->id);

    object_id id = 0;
    auto C = w.ensure_player_character(id);
    fm_assert(C->id == id);
    fm_assert(id == a->id);
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

    test_dt_invariance();
    test_offset_frac_diag_to_cardinal();
    test_move_toward_line();
    test_move_toward_line_blocked();
    test_move_toward_line_displaced();
    test_walk_smoothed();
    test_walk_smoothed_blocked();
    test_walk_smoothed_displaced();
    test_walk_interrupted();
    test_player_character_id_lowest();

    if (is_noisy)
    {
        std::fputc('\t', stdout);
        std::fflush(stdout);
    }
}

} // namespace floormat
