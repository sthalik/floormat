#include "app.hpp"
#include "run.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/anim-atlas.hpp"
#include "src/critter-script-cover.hpp"
#include "src/grid-registry.hpp"
#include "src/order-list.inl"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/search-astar.hpp"
#include "src/update-world.hpp"
#include "src/point.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/nanosecond.inl"
#include <array>
#include <mg/Functions.h>

namespace floormat::Test {

namespace {

using state = Cover::Script::status::state;

constexpr auto chunk_a = chunk_coords_{0, 0, 0};
constexpr chunk_coords_ chunks[] = { chunk_a };
constexpr auto start = point{chunk_a, {8, 8}, {}}, west_of_start = point{chunk_a, {3, 8}, {}};
constexpr auto dt_several_steps = 100*Millisecond;

bptr<critter> make_critter(world& w, point pos, bool playable)
{
    auto proto = Run::make_proto(1.f);
    proto.playable = playable;
    proto.offset = pos.offset();
    return w.make_object<critter>(w.make_id(), pos.coord(), move(proto));
}

struct movement_state
{
    global_coords coord;
    Vector2b offset;
    rotation r;
    u8 moves;
    u32 delta;
    u16 frame;
};

movement_state movement_state_of(const critter& c)
{
    return {c.coord, c.offset, c.r, c.moves_, c.delta, c.frame};
}

void assert_same_movement_state(const movement_state& a, const movement_state& b)
{
    fm_assert(a.coord == b.coord);
    fm_assert(a.offset == b.offset);
    fm_assert(a.r == b.r);
    fm_assert_equal(a.moves, b.moves);
    fm_assert_equal(a.delta, b.delta);
    fm_assert_equal(a.frame, b.frame);
}

// Thinks for C alone, asserts that C didn't change and gave the one order of kind, then runs that order.
void think_then_run_order(const bptr<critter>& C, order_kind kind, const Ns& dt)
{
    order_buffer buffer;
    order_list list;
    const auto before = movement_state_of(*C);
    prepare_world_for_thinking(C->world());
    auto ctx = think_context{buffer, C->world().grid_orders()};
    C->think(dt, ctx, 0);
    assert_same_movement_state(before, movement_state_of(*C));
    list.append_buffers({&buffer, 1});
    fm_assert_equal(1u, list.size());
    fm_assert(list[0].kind == kind);
    fm_assert(list[0].giver == C->id);
    list.run_all([&](const order& o, u32) { run_order(C->world(), o, dt); });
}

void test_walk_script_order()
{
    auto w1 = world(), w2 = world();
    (void)w1[chunk_a];
    (void)w2[chunk_a];
    const auto C1 = make_critter(w1, start, false), C2 = make_critter(w2, start, false);
    fm_assert(C1->id == C2->id);
    w1.init_scripts();
    w2.init_scripts();
    C1->script.do_reassign(critter_script::make_walk_script(west_of_start), C1);
    C2->script.do_reassign(critter_script::make_walk_script(west_of_start), C2);

    think_then_run_order(C1, order_kind::script_order, dt_several_steps);
    auto i = C2->index();
    C2->update(C2, i, dt_several_steps);
    fm_assert(C1->position() != start);
    assert_same_movement_state(movement_state_of(*C1), movement_state_of(*C2));
    fm_assert(C1->last_walk == C2->last_walk);
    fm_assert(C1->script->name() == "walk"_s && C2->script->name() == "walk"_s);
    w1.finish_scripts();
    w2.finish_scripts();
}

void test_move_by_keys_order()
{
    auto w1 = world(), w2 = world();
    (void)w1[chunk_a];
    (void)w2[chunk_a];
    const auto C1 = make_critter(w1, start, true), C2 = make_critter(w2, start, true);
    w1.init_scripts();
    w2.init_scripts();
    C1->set_keys(false, true, false, true);
    C2->set_keys(false, true, false, true);

    think_then_run_order(C1, order_kind::move_by_keys, dt_several_steps);
    auto i = C2->index();
    C2->update(C2, i, dt_several_steps);
    fm_assert(C1->position() != start);
    assert_same_movement_state(movement_state_of(*C1), movement_state_of(*C2));
    w1.finish_scripts();
    w2.finish_scripts();
}

// The held keys stop auto movement, so one update() ends the walk script and moves C the way the keys point.
void test_keys_end_walk_script()
{
    auto w = world();
    (void)w[chunk_a];
    const auto C = make_critter(w, start, true);
    w.init_scripts();
    C->script.do_reassign(critter_script::make_walk_script(west_of_start), C);
    fm_assert(C->moves.AUTO);
    C->set_keys(false, true, false, true);
    auto i = C->index();
    C->update(C, i, dt_several_steps);
    fm_assert(C->script->name() == "empty"_s);
    fm_assert(C->last_walk == critter::walk_result::none);
    fm_assert(!C->moves.AUTO);
    const auto moved = C->position() - start;
    fm_assert(moved.x() > 0 && moved.y() == 0);
    w.finish_scripts();
}

door_scenery& add_closed_door(world& w)
{
    auto proto = loader.scenery("door1");
    proto.pass = pass_mode::blocked;
    proto.frame = (u16)(proto.atlas->info().nframes - 1);
    auto door = w.make_scenery(w.make_id(), {chunk_a, {8, 8}}, move(proto));
    fm_assert(door->scenery_type() == scenery_type::door);
    auto& d = static_cast<door_scenery&>(*door);
    auto i = d.index();
    fm_assert(d.activate(i));
    return d;
}

// update_world() in w1, an update_object order in w2 and door_scenery::update() in w3 open the same door alike.
void test_door_update_object_order()
{
    auto w1 = world(), w2 = world(), w3 = world();
    auto &d1 = add_closed_door(w1), &d2 = add_closed_door(w2), &d3 = add_closed_door(w3);
    const auto frame = Second/60;
    const auto first_frame = d1.frame;
    for (auto n = 0u; d3.active; n++)
    {
        fm_assert(n < 1000);
        update_world(w1, chunks, frame);
        for (auto* w : { &w2, &w3 })
        {
            w->increment_frame_no();
            w->advance_game_time(frame);
        }
        run_order(w2, {.giver = d2.id, .kind = order_kind::update_object}, frame);
        auto i = d3.index();
        d3.update(w3.find_object(d3.id), i, frame);
        for (const auto* d : { &d1, &d2 })
        {
            fm_assert_equal(d3.frame, d->frame);
            fm_assert_equal(d3.active, d->active);
            fm_assert(d3.pass == d->pass);
        }
    }
    fm_assert(d1.frame != first_frame);
}

// Records in on_think where the watched critter stands.
struct position_recording_script final : critter_script
{
    object_id watched_id;
    point& watched_position;

    position_recording_script(object_id watched_id, point& watched_position) :
        watched_id{watched_id}, watched_position{watched_position}
    {}

    StringView name() const override { return "position-recording"_s; }
    const void* id() const override { return &watched_id; }
    void on_init(const bptr<critter>&) override {}
    void on_think(const critter& c, const Ns&, think_context&) override
    {
        const auto watched = c.world().find_object<critter>(watched_id);
        fm_assert(watched);
        watched_position = watched->position();
    }
    void on_order(const bptr<critter>&, usize&, const Ns&, const order&) override { fm_assert(false); }
    void on_destroy(const bptr<critter>&, script_destroy_reason) override {}
    void delete_self() noexcept override { delete this; }
};

void test_think_sees_world_from_before_frame()
{
    auto w = world();
    (void)w[chunk_a];
    const auto A = make_critter(w, point{chunk_a, {8, 4}, {}}, false);
    const auto B = make_critter(w, point{chunk_a, {8, 12}, {}}, false);
    fm_assert(A->index() < B->index());
    w.init_scripts();
    A->script.do_reassign(critter_script::make_walk_script(point{chunk_a, {3, 4}, {}}), A);
    point seen_by_B;
    B->script.do_reassign(Pointer<critter_script>{new position_recording_script{A->id, seen_by_B}}, B);
    const auto A_start = A->position();
    update_world(w, chunks, dt_several_steps);
    fm_assert(A->position() != A_start);
    fm_assert(seen_by_B == A_start);
    w.finish_scripts();
}

// Not square, since rotate() swaps bbox_size's x and y, so a turn during on_think() changes what others read.
constexpr auto body = Vector2ui{30, 24};
constexpr u32 cover_div_size = 8, num_cover_npcs = 4;

// A threat and four cover npcs west of two walls. The cover grids have all 32 octants filled before the first
// frame, so no search's octant cap depends on what an earlier npc filled.
struct cover_npcs_world
{
    world w;
    astar A;
    bptr<critter> threat;
    std::array<bptr<critter>, num_cover_npcs> npcs;
    u32 octant_fills_after_prefill = 0;

    cover_npcs_world();
    ~cover_npcs_world() noexcept;
    Grid::Cover::Pool& cover_pool();
    bptr<critter> add_critter(point pos);
    bptr<scenery> add_wall(local_coords tile, Vector2ub bbox_size);
    void run_frame(Ns dt, bool is_reverse_order, order_buffer& buffer, order_list& list);
};

cover_npcs_world::cover_npcs_world()
{
    auto& c = w[chunk_a];
    const auto ground = tile_image_proto{loader.ground_atlas("tiles"), 0};
    for (auto j = 0u; j < TILE_MAX_DIM; j++)
        for (auto i = 0u; i < TILE_MAX_DIM; i++)
            c[{i, j}].ground() = ground;
    add_wall({7, 8}, {16, 96});
    add_wall({7, 12}, {16, 96});
    c.mark_passability_modified();
    c.ensure_passability();
    threat = add_critter(point{chunk_a, {12, 8}, {}});
    const point npc_positions[] = {
        {chunk_a, {4, 7}, {}}, {chunk_a, {4, 9}, {}}, {chunk_a, {3, 8}, {}}, {chunk_a, {5, 10}, {}},
    };
    for (auto k = 0u; k < num_cover_npcs; k++)
        npcs[k] = add_critter(npc_positions[k]);
    w.set_grid_registry(&A.pool_registry());
    w.init_scripts();
    for (const auto& npc : npcs)
        npc->script.do_reassign(critter_script::make_cover_script(A, threat->id, cover_div_size), npc);

    auto& dijkstra_pool = A.pool_registry().object_pool(
        Grid::Pass::Params{astar::default_div_size, Math::max(body.x(), body.y()), pass_mode::pass});
    auto& ray_pool = A.pool_registry().pass_pool(Grid::Pass::Params{cover_div_size, pass_mode::shoot_through});
    for (auto& ch : w.chunks())
    {
        dijkstra_pool[ch].build_if_stale();
        dijkstra_pool.pass_pool()[ch].build_if_stale();
        ray_pool[ch].build_if_stale();
        auto g = cover_pool()[ch];
        g.build_if_stale();
        for (auto k = 0u; k < Grid::Cover::octant_count; k++)
            (void)g.ensure_octant(k);
        fm_assert_equal((u32)-1, g.built_octants());
    }
    octant_fills_after_prefill = cover_pool().octant_fills();
}

cover_npcs_world::~cover_npcs_world() noexcept { w.finish_scripts(); }

Grid::Cover::Pool& cover_npcs_world::cover_pool()
{
    return A.pool_registry().cover_pool(Grid::Cover::Params{cover_div_size});
}

bptr<critter> cover_npcs_world::add_critter(point pos)
{
    critter_proto p;
    p.name = "npc"_s;
    p.bbox_size = Vector2ub(body);
    p.offset = pos.offset();
    return w.make_object<critter>(w.make_id(), pos.coord(), move(p));
}

bptr<scenery> cover_npcs_world::add_wall(local_coords tile, Vector2ub bbox_size)
{
    scenery_proto p;
    p.atlas = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype = generic_scenery_proto{};
    p.bbox_size = bbox_size;
    p.pass = pass_mode::blocked;
    return w.make_scenery(w.make_id(), {chunk_a, tile}, move(p));
}

// npc k thinks under update_index k, so the orders run in the same order either way.
void cover_npcs_world::run_frame(Ns dt, bool is_reverse_order, order_buffer& buffer, order_list& list)
{
    w.increment_frame_no();
    w.advance_game_time(dt);
    prepare_world_for_thinking(w);
    for (auto n = 0u; n < num_cover_npcs; n++)
    {
        const auto k = is_reverse_order ? num_cover_npcs - 1 - n : n;
        auto ctx = think_context{buffer, w.grid_orders()};
        npcs[k]->think(dt, ctx, k);
    }
    list.append_buffers({&buffer, 1});
    list.run_all([&](const order& o, u32) { run_order(w, o, dt); });
    list.clear();
    buffer.clear();
}

// Cover npcs that think in reverse order walk and change state as those thinking in update order do.
void test_cover_npcs_think_order()
{
    cover_npcs_world s1, s2;
    order_buffer buffer;
    order_list list;
    const auto frame = Second / s1.npcs[0]->atlas->info().fps;
    const point threat_positions[] = { {chunk_a, {12, 4}, {}}, {chunk_a, {12, 13}, {}} };
    std::array<point, num_cover_npcs> start_positions;
    for (auto k = 0u; k < num_cover_npcs; k++)
        start_positions[k] = s1.npcs[k]->position();
    std::array<bool, num_cover_npcs> has_moved{};
    auto has_hidden = false;
    for (auto n = 0u; n < 900; n++)
    {
        if (n % 300 == 299)
            for (auto* s : { &s1, &s2 })
            {
                auto i = s->threat->index();
                s->threat->teleport_to(i, threat_positions[n / 300 % 2], rotation_COUNT);
            }
        s1.run_frame(frame, false, buffer, list);
        s2.run_frame(frame, true, buffer, list);
        for (auto k = 0u; k < num_cover_npcs; k++)
        {
            const auto &a = *s1.npcs[k], &b = *s2.npcs[k];
            fm_assert(a.coord == b.coord);
            fm_assert(a.offset == b.offset);
            fm_assert(a.r == b.r);
            const auto st = Cover::Script::get_status(*s1.npcs[k])->st;
            fm_assert(st == Cover::Script::get_status(*s2.npcs[k])->st);
            has_moved[k] |= a.position() != start_positions[k];
            has_hidden |= st == state::hidden;
        }
        fm_assert_equal(s1.octant_fills_after_prefill, s1.cover_pool().octant_fills());
        fm_assert_equal(s2.octant_fills_after_prefill, s2.cover_pool().octant_fills());
    }
    fm_assert(has_hidden);
    for (auto k = 0u; k < num_cover_npcs; k++)
        fm_assert(has_moved[k]);
}

} // namespace

void test_think_orders()
{
    test_walk_script_order();
    test_move_by_keys_order();
    test_keys_end_walk_script();
    test_door_update_object_order();
    test_think_sees_world_from_before_frame();
    test_cover_npcs_think_order();
}

} // namespace floormat::Test
