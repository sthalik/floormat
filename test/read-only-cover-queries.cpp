#include "app.hpp"
#include "grid-read-only.hpp"
#include "src/critter-script-cover.hpp"
#include "src/critter-script-cover-movement-range.hpp"
#include "src/critter-script-cover-point.hpp"
#include "src/critter-walk.hpp"
#include "src/critter.hpp"
#include "src/chunk.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-object.hpp"
#include "src/grid-order.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-registry.hpp"
#include "src/raycast.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/search-pred.hpp"
#include "src/world.hpp"
#include "src/point.inl"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "compat/array-size.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/floor-divmod.hpp"
#include "compat/function-view.hpp"
#include <algorithm>
#include <bit>
#include <utility>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>

namespace floormat::Test {

namespace {

using Cover::Point::finder;
using Cover::Point::query;
using Cover::Point::result;
using Cover::Point::spot;
using Raycast::raycast_result_s;

constexpr chunk_coords_ COORD{0, 0, 0};
constexpr auto cover_params = Grid::Cover::Params{16};
constexpr auto cover_pass_params = Pass::Params{cover_params.div_size, pass_mode::shoot_through};
constexpr u32 los_div_size = 8;
constexpr auto los_params = Pass::Params{los_div_size, pass_mode::shoot_through};
constexpr u32 body_side = 24;
constexpr auto body_size = Vector2ui{body_side};
// movement_range's pool for body_side, which the walk path check uses too.
constexpr auto walk_params = Pass::Params{astar::default_div_size, body_side, pass_mode::pass};

point at(Vector2i px)
{
    return point::normalize_coords(point{COORD, {}, {}}, px);
}

u64 bits_of(collision_data x)
{
    return std::bit_cast<u64>(x);
}

// Creates the pools the calls below use, so that a const registry finds them.
void add_pools(Grid::GridRegistry& reg)
{
    (void)reg.cover_pool(cover_params);
    (void)reg.object_pool(walk_params);
    (void)reg.pass_pool(los_params);
}

struct const_pools
{
    const Grid::Cover::Pool& cover;
    const Grid::Object::Pool& objects;
    const Pass::Pool& walk;
    const Pass::Pool& los;
    const Pass::Pool& cover_pass;
};

const_pools pools_of(const Grid::GridRegistry& reg)
{
    const auto* cover = reg.find_cover_pool(cover_params);
    const auto* objects = reg.find_object_pool(walk_params);
    const auto* walk = reg.find_pass_pool(walk_params);
    const auto* los = reg.find_pass_pool(los_params);
    const auto* cover_pass = reg.find_pass_pool(cover_pass_params);
    fm_assert(cover && objects && walk && los && cover_pass);
    return { *cover, *objects, *walk, *los, *cover_pass };
}

void record_state(const world& w, const const_pools& P, Array<u64>& out)
{
    const Pass::Pool* const pass_pools[] = { &P.walk, &P.los, &P.cover_pass };
    const Grid::Object::Pool* const object_pools[] = { &P.objects };
    const Grid::Cover::Pool* const cover_pools[] = { &P.cover };
    Test::record_state(w, pass_pools, object_pools, cover_pools, out);
}

void add_blocker(world& w, point pt)
{
    scenery_proto p;
    p.atlas     = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype   = generic_scenery_proto{};
    p.offset    = pt.offset();
    p.bbox_size = Vector2ub{32};
    p.pass      = pass_mode::blocked;
    (void)w.make_scenery(w.make_id(), pt.coord(), move(p));
    rebuild_passability(w[pt.chunk3()]);
}

struct test_critter_ids
{
    object_id walker, mover, edge_mover, wall_mover, missing_chunk_mover;
};

struct test_world
{
    world w;
    pred_object_ids ids;
    test_critter_ids critters;
};

constexpr Vector2i walker_from{-100, 700}, walker_to{1500, 900};

// Returns make_world_with_ground() with add_colliders() and the critters the tests below move.
test_world make_test_world()
{
    auto w = make_world_with_ground();
    const auto ids = add_colliders(w);
    const auto add = [&](Vector2i px) { return add_critter_at(w, px, (u8)body_side, pass_mode::blocked)->id; };
    const auto critters = test_critter_ids{
        .walker = add(walker_from), .mover = add({700, 60}), .edge_mover = add({980, 60}),
        .wall_mover = add({550, 450}), .missing_chunk_mover = add({2000, 100}),
    };
    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
            rebuild_passability(w[chunk_coords_{x, y, 0}]);
    return { move(w), ids, critters };
}

struct finder_case
{
    query q;
    Search::pred pred;
    bool list_spots;
};

Array<finder_case> make_finder_cases()
{
    Array<finder_case> cases;
    arrayReserve(cases, 8);
    const auto add = [&](Vector2i npc, Vector2i threat, u32 radius, u8 octants, u32 max_cover_end_dist,
                         Optional<Range2Di> area, const Search::pred& pred, bool list_spots) {
        const auto q = query{ at(npc), body_size, at(threat), radius, octants, max_cover_end_dist, area };
        arrayAppend(cases, finder_case{ q, pred, list_spots });
    };
    add({600, 560}, {600, 150}, 192, 0, 0, {}, Search::without_critters, false);
    add({600, 560}, {600, 150}, 192, 0, 0, {}, Search::without_critters, true);
    add({600, 560}, {600, 150}, 192, 3, 64, {}, Search::never_continue, true);
    add({200, 300}, {600, 300}, 160, 0, 0, {}, Search::never_continue, true);
    add({1000, 250}, {1300, 250}, 160, 0, 32, Range2Di{{-160, -160}, {0, 160}}, Search::without_critters, true);
    add({-100, 900}, {-600, 1400}, 224, 5, 0, {}, Search::without_critters, true);
    return cases;
}

result find_today(finder& F, world& w, Grid::GridRegistry& reg, const finder_case& c)
{
    return F.find(w, reg, cover_params, c.q, c.pred, c.list_spots);
}

result find_read_only(finder& F, const world& w, const const_pools& P, const finder_case& c,
                      Array<grid_order>& orders)
{
    return F.find(w, P.cover, c.q, c.pred, orders, c.list_spots);
}

spot spot_of(const query& q)
{
    return { q.pos, Cover::Point::axis_toward(q.threat - q.pos) };
}

void assert_same_result(const result& a, const result& b, bool list_spots)
{
    fm_assert_equal(a.found, b.found);
    fm_assert(a.pos == b.pos);
    fm_assert(a.axis == b.axis);
    fm_assert_equal(a.candidates, b.candidates);
    fm_assert_equal(a.rejected, b.rejected);
    fm_assert_equal(a.out_of_fills, b.out_of_fills);
    if (list_spots)
    {
        fm_assert_equal(a.spots.size(), b.spots.size());
        for (u32 i = 0; i < (u32)a.spots.size(); i++)
        {
            fm_assert(a.spots[i].pos == b.spots[i].pos);
            fm_assert(a.spots[i].axis == b.spots[i].axis);
        }
    }
}

struct range_case
{
    Vector2i from;
    // True when the critter add_colliders() puts at `from` is in excluded_blocker_ids.
    bool is_from_critter_excluded;
    u32 max_dist, radius, spacing;
};

constexpr range_case range_cases[] = {
    { {600, 560}, false, 128, 96, 32 },
    { {992, 992}, true, 160, 128, 32 },
    { {1000, 40}, false, 144, 112, 16 },
};

std::array<object_id, 2> excluded_ids_of(const range_case& c, const test_world& t)
{
    return { c.is_from_critter_excluded ? t.ids.critter_id : 0, 0 };
}

void assert_same_points(ArrayView<const point> a, ArrayView<const point> b)
{
    fm_assert_equal(a.size(), b.size());
    for (u32 i = 0; i < (u32)a.size(); i++)
        fm_assert(a[i] == b[i]);
}

// Asserts that orders hold one order of the given kind for each existing chunk of the (2n + 1)² cells around
// from's cell.
void check_movement_range_orders(const world& w, ArrayView<const grid_order> orders, point from, int n,
                                 grid_order::grid_kind kind)
{
    const auto div = (int)walk_params.div_size(), dc = (int)walk_params.div_count;
    const auto g = Vector3i(from).xy();
    const auto cell_lo = Vector2i{floor_div(g.x() + half_tile<int>, div), floor_div(g.y() + half_tile<int>, div)}
                         - Vector2i{n};
    const auto cell_hi = cell_lo + Vector2i{2*n};
    u32 num_chunks = 0;
    for (auto cy = floor_div(cell_lo.y(), dc); cy <= floor_div(cell_hi.y(), dc); cy++)
        for (auto cx = floor_div(cell_lo.x(), dc); cx <= floor_div(cell_hi.x(), dc); cx++)
        {
            const auto ch = chunk_coords_{(i16)cx, (i16)cy, from.chunk3().z};
            if (!w.at(ch))
                continue;
            num_chunks++;
            u32 num_orders = 0;
            for (const auto& o : orders)
                num_orders += o.chunk == ch;
            fm_assert_equal(1u, num_orders);
        }
    fm_assert_equal(num_chunks, (u32)orders.size());
    for (const auto& o : orders)
    {
        fm_assert(o.kind == kind);
        fm_assert(o.pass_params == walk_params);
    }
}

constexpr Vector2i move_deltas[] = { {20, 0}, {0, 20}, {30, 0}, {0, -30}, {30, 0} };

// Moves within a chunk, across a chunk edge, into a wall and into the missing chunk (2, 0, 0).
std::array<object_id, 5> movers_of(const test_world& t)
{
    const auto& m = t.critters;
    return { m.mover, m.mover, m.edge_mover, m.wall_mover, m.missing_chunk_mover };
}

constexpr chunk_coords_ missing_chunk{2, 0, 0};

struct segment
{
    Vector2i from, to;
};

constexpr segment los_segments[] = {
    {{-900, 100}, {1900, 700}},
    {{100, -900}, {300, 1900}},
    {{600, 560}, {600, 150}},
    {{40, 600}, {1900, 640}},
    {{-950, -950}, {1950, 1950}},
};

raycast_result_s line_of_sight_today(world& w, Grid::GridRegistry& reg, const segment& s)
{
    return Cover::Script::line_of_sight(w, reg, los_div_size, at(s.from), at(s.to), 0);
}

raycast_result_s line_of_sight_read_only(const world& w, const const_pools& P, const segment& s,
                                         Array<grid_order>& orders)
{
    return Cover::Script::line_of_sight(w, P.los, at(s.from), at(s.to), 0, orders);
}

void assert_same_ray(const raycast_result_s& a, const raycast_result_s& b)
{
    fm_assert_equal((bool)a.success, (bool)b.success);
    fm_assert_equal(bits_of(a.collider), bits_of(b.collider));
    fm_assert(a.collision == b.collision);
}

void check_pass_orders(ArrayView<const grid_order> orders, const Pass::Params& params)
{
    for (const auto& o : orders)
    {
        fm_assert(o.kind == grid_order::grid_kind::pass);
        fm_assert(o.pass_params == params);
        fm_assert(o.chunk.x >= -1 && o.chunk.x <= 1 && o.chunk.y >= -1 && o.chunk.y <= 1 && o.chunk.z == 0);
    }
}

struct walk_along_path
{
    critter_walk walk;
    // The middle of the path's longest smoothed leg, away from where the walker stands.
    point longest_leg_middle;
};

// Returns a walk from the walker's position to walker_to, along a path smoothed in w.
walk_along_path make_walk(astar& A, world& w, Grid::GridRegistry& reg, const test_world& t)
{
    auto& pool = reg.pass_pool(walk_params);
    const auto& walker = *w.find_object<critter>(t.critters.walker);
    auto path = A.Dijkstra(w, pool, walker.position(), at(walker_to), 8192, body_size, Search::without_critters);
    fm_assert(path.is_found());
    (void)A.string_pull(w, pool, path, body_size, Search::without_critters);
    const auto sp = std::as_const(path).smoothings();
    fm_assert(sp.size() >= 3);
    fm_assert(sp[0].pt == walker.position());
    auto longest = 1u;
    for (u32 j = 2; j < (u32)sp.size(); j++)
        if ((sp[j].pt - sp[j-1].pt).dot() > (sp[longest].pt - sp[longest-1].pt).dot())
            longest = j;
    fm_assert((sp[longest].pt - sp[longest-1].pt).dot() > 128*128);
    const auto middle = sp[longest-1].pt + (sp[longest].pt - sp[longest-1].pt)/2;
    return { critter_walk{move(path)}, middle };
}

// The walk path check with a pool and a pred letting critters through, then without a pool, as
// critter_walk::is_remaining_path_unblocked(critter&) calls it.
struct path_check_results
{
    bool with_pool, without_pool;
};

path_check_results check_path_today(const critter_walk& walk, world& w, Grid::GridRegistry& reg, object_id id)
{
    auto& c = *w.find_object<critter>(id);
    auto& pool = reg.pass_pool(walk_params);
    return { walk.is_remaining_path_unblocked(c, Search::without_critters, &pool),
             walk.is_remaining_path_unblocked(c) };
}

path_check_results check_path_read_only(const critter_walk& walk, const world& w, const const_pools& P,
                                        object_id id, Array<grid_order>& pool_orders,
                                        Array<grid_order>& no_pool_orders)
{
    const auto& c = *w.find_object<critter>(id);
    return { walk.is_remaining_path_unblocked(c, Search::without_critters, &P.walk, pool_orders),
             walk.is_remaining_path_unblocked(c, Search::blocks_all_but(id), nullptr, no_pool_orders) };
}

// Test 2: read-only calls on a world with some grids built, some stale and some missing change nothing.
void test_read_only_cover_writes_nothing()
{
    auto t = make_test_world();
    auto& w = t.w;
    Grid::GridRegistry reg;
    add_pools(reg);
    finder F;
    const auto cases = make_finder_cases();
    auto c0 = cases[0];
    c0.q.max_fills = 5;
    (void)find_today(F, w, reg, c0);
    (void)Cover::movement_range{}.border(w, reg, at(range_cases[0].from), body_side, range_cases[0].max_dist);
    (void)line_of_sight_today(w, reg, los_segments[2]);
    astar A;
    const auto walk = make_walk(A, w, reg, t);
    // Makes stale the grids built for chunk (0, 1, 0) and its neighbors.
    w[chunk_coords_{0, 1, 0}][local_coords{8, 8}].wall_north() = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    rebuild_passability(w[chunk_coords_{0, 1, 0}]);

    const auto& cw = w;
    const auto P = pools_of(reg);
    Array<u64> before, after;
    record_state(cw, P, before);
    Array<grid_order> orders, no_pool_orders;
    for (const auto& c : cases)
    {
        (void)find_read_only(F, cw, P, c, orders);
        const auto s = spot_of(c.q);
        (void)F.is_hidden_at(cw, c.q.threat, s, body_size);
        (void)F.is_hidden_from_threat_in_neighbor_octants(cw, c.q.threat, s, body_size, 3);
        (void)F.move_to_cover_end(cw, c.q, s.axis, c.pred);
    }
    for (const auto& c : range_cases)
    {
        const auto ids = excluded_ids_of(c, t);
        (void)Cover::movement_range{}.border(cw, P.walk, at(c.from), body_side, c.max_dist, orders);
        (void)Cover::movement_range{}.reachable_lattice_points(cw, P.objects, at(c.from), ids, body_side, c.radius,
                                                               c.spacing, orders);
        (void)Cover::movement_range{}.reachable_lattice_points_without_critters(cw, P.objects, at(c.from),
                                                                                body_side, c.radius, c.spacing,
                                                                                orders);
    }
    const auto movers = movers_of(t);
    for (u32 i = 0; i < array_size(move_deltas); i++)
    {
        const auto& c = *cw.find_object<critter>(movers[i]);
        (void)c.can_move_to(move_deltas[i]);
        (void)c.can_rotate(rotation::E);
    }
    (void)check_path_read_only(walk.walk, cw, P, t.critters.walker, orders, no_pool_orders);
    for (const auto& s : los_segments)
        (void)line_of_sight_read_only(cw, P, s, orders);
    (void)Cover::Script::find_object_at_z(cw, t.critters.walker, 0);
    record_state(cw, P, after);

    fm_assert(!orders.isEmpty());
    fm_assert(!cw.at(missing_chunk));
    fm_assert_equal(before.size(), after.size());
    for (u32 i = 0; i < (u32)before.size(); i++)
        fm_assert_equal(before[i], after[i]);
}

// Fills every octant of every chunk's cover grid.
void fill_all_octants(world& w, Grid::GridRegistry& reg)
{
    auto& pool = reg.cover_pool(cover_params);
    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
        {
            auto g = pool[w[chunk_coords_{x, y, 0}]];
            for (u32 k = 0; k < 32; k++)
                (void)g.ensure_octant(k);
        }
}

// Test 3: once today's overloads ran and every octant is filled, read-only results equal theirs, with no order.
void test_read_only_cover_with_built_grids()
{
    auto t = make_test_world();
    auto& w = t.w;
    const auto& cw = w;
    Grid::GridRegistry reg;
    add_pools(reg);
    Array<grid_order> orders, no_pool_orders;

    const auto cases = make_finder_cases();
    finder F, F_read_only;
    for (const auto& c : cases)
        (void)find_today(F, w, reg, c);
    fill_all_octants(w, reg);
    const auto P = pools_of(reg);
    u32 num_found = 0;
    for (const auto& c : cases)
    {
        // A result's spots stay valid until its finder's next find(), so today's is found again.
        const auto expected = find_today(F, w, reg, c);
        const auto r = find_read_only(F_read_only, cw, P, c, orders);
        assert_same_result(expected, r, c.list_spots);
        fm_assert_equal(0u, r.fills);
        fm_assert(orders.isEmpty());
        num_found += r.found;
        const auto s = spot_of(c.q);
        fm_assert_equal(F.is_hidden_at(w, c.q.threat, s, body_size),
                        F_read_only.is_hidden_at(cw, c.q.threat, s, body_size));
        fm_assert_equal(F.is_hidden_from_threat_in_neighbor_octants(w, c.q.threat, s, body_size, 3),
                        F_read_only.is_hidden_from_threat_in_neighbor_octants(cw, c.q.threat, s, body_size, 3));
        fm_assert(F.move_to_cover_end(w, c.q, s.axis, c.pred) ==
                  F_read_only.move_to_cover_end(cw, c.q, s.axis, c.pred));
    }
    fm_assert(num_found >= 3);

    for (const auto& c : range_cases)
    {
        const auto ids = excluded_ids_of(c, t);
        Cover::movement_range today, read_only;
        auto expected = today.border(w, reg, at(c.from), body_side, c.max_dist);
        assert_same_points(expected, read_only.border(cw, P.walk, at(c.from), body_side, c.max_dist, orders));
        expected = today.reachable_lattice_points(w, reg, at(c.from), ids, body_side, c.radius, c.spacing);
        assert_same_points(expected,
                           read_only.reachable_lattice_points(cw, P.objects, at(c.from), ids, body_side, c.radius,
                                                              c.spacing, orders));
        expected = today.reachable_lattice_points_without_critters(w, reg, at(c.from), body_side, c.radius,
                                                                   c.spacing);
        assert_same_points(expected,
                           read_only.reachable_lattice_points_without_critters(cw, P.objects, at(c.from),
                                                                               body_side, c.radius, c.spacing,
                                                                               orders));
        fm_assert(orders.isEmpty());
    }

    const auto movers = movers_of(t);
    u32 num_moves_possible = 0;
    for (u32 i = 0; i + 1 < array_size(move_deltas); i++)
    {
        auto& c = *w.find_object<critter>(movers[i]);
        const auto& cc = std::as_const(c);
        const auto can_move = c.can_move_to(move_deltas[i]);
        fm_assert_equal(can_move, cc.can_move_to(move_deltas[i]));
        fm_assert_equal(c.can_rotate(rotation::E), cc.can_rotate(rotation::E));
        num_moves_possible += can_move;
    }
    fm_assert(!w.find_object<critter>(t.critters.wall_mover)->can_move_to(move_deltas[3]));
    fm_assert(num_moves_possible >= 2);

    astar A;
    for (const auto& s : los_segments)
    {
        const auto r = line_of_sight_today(w, reg, s);
        assert_same_ray(r, line_of_sight_read_only(cw, P, s, orders));
        fm_assert(orders.isEmpty());
    }
    fm_assert(Cover::Script::find_object_at_z(cw, t.critters.walker, 0) == &*w.find_object(t.critters.walker));
    fm_assert(!Cover::Script::find_object_at_z(cw, t.critters.walker, 1));

    const auto walk = make_walk(A, w, reg, t);
    for (u32 is_blocked = 0; is_blocked < 2; is_blocked++)
    {
        if (is_blocked)
            add_blocker(w, walk.longest_leg_middle);
        const auto expected_checks = check_path_today(walk.walk, w, reg, t.critters.walker);
        const auto checks = check_path_read_only(walk.walk, cw, P, t.critters.walker, orders, no_pool_orders);
        fm_assert_equal(expected_checks.with_pool, checks.with_pool);
        fm_assert_equal(expected_checks.without_pool, checks.without_pool);
        fm_assert_equal(!is_blocked, checks.with_pool);
        fm_assert(orders.isEmpty() && no_pool_orders.isEmpty());
    }
}

// Runs the orders of a read-only finder through the writable cover pool.
void run_cover_orders(world& w, Grid::Cover::Pool& pool, ArrayView<const grid_order> orders)
{
    for (const auto& o : orders)
    {
        auto* c = w.at(o.chunk);
        fm_assert(c);
        auto g = pool[*c];
        g.build_if_stale();
        (void)g.ensure_octant(o.octant);
    }
}

struct chunk_range
{
    chunk_coords_ lo, hi;
};

// Returns the chunks of the cells a finder query reads, within q.radius of q.pos plus a body and a cell.
chunk_range finder_chunks_of(const query& q)
{
    const auto body = Math::max(q.body_size.x(), q.body_size.y());
    const auto margin = Vector2i{(int)(q.radius + body + cover_params.div_size)};
    const auto P = Vector3i(q.pos).xy();
    const auto lo = P - margin, hi = P + margin;
    const auto chunk_of = [](int px) { return (i16)floor_div(px + half_tile<int>, chunk_size<int>); };
    const auto z = q.pos.chunk3().z;
    return { { chunk_of(lo.x()), chunk_of(lo.y()), z }, { chunk_of(hi.x()), chunk_of(hi.y()), z } };
}

u32 count_existing_chunks(const world& w, const chunk_range& r)
{
    u32 n = 0;
    for (auto y = r.lo.y; y <= r.hi.y; y++)
        for (auto x = r.lo.x; x <= r.hi.x; x++)
            n += w.at(chunk_coords_{x, y, r.lo.z}) != nullptr;
    return n;
}

void check_cover_orders(ArrayView<const grid_order> orders, const chunk_range& r)
{
    for (const auto& o : orders)
    {
        fm_assert(o.kind == grid_order::grid_kind::cover);
        fm_assert_equal(cover_params.div_size, o.cover_div_size);
        fm_assert(o.pass_params == cover_pass_params);
        fm_assert(o.octant < 32);
        fm_assert_equal(r.lo.z, o.chunk.z);
        fm_assert(o.chunk.x >= r.lo.x && o.chunk.x <= r.hi.x && o.chunk.y >= r.lo.y && o.chunk.y <= r.hi.y);
    }
}

// Test 4: without grids, read-only results equal today's on an identical copy, and their orders are what the
// rules for grid_order say. The finder's orders, once run, let it reach today's result.
void test_read_only_cover_without_grids()
{
    auto ta = make_test_world(), tb = make_test_world();
    fm_assert(ta.ids.critter_id == tb.ids.critter_id);
    fm_assert(ta.critters.missing_chunk_mover == tb.critters.missing_chunk_mover);
    auto &a = ta.w, &b = tb.w;
    const auto& cb = b;
    Grid::GridRegistry reg_a, reg_b;
    add_pools(reg_b);
    const auto Pb = pools_of(reg_b);
    Array<grid_order> orders, no_pool_orders;

    for (const auto& c : range_cases)
    {
        const auto ids = excluded_ids_of(c, ta);
        const auto from = at(c.from);
        const auto border_n = (int)c.max_dist/(int)walk_params.div_size() + 1;
        const auto lattice_n = ((int)c.radius + (int)walk_params.div_size() - 1)/(int)walk_params.div_size();
        Cover::movement_range today, read_only;
        assert_same_points(today.border(a, reg_a, from, body_side, c.max_dist),
                           read_only.border(cb, Pb.walk, from, body_side, c.max_dist, orders));
        check_movement_range_orders(cb, orders, from, border_n, grid_order::grid_kind::pass);
        arrayClear(orders);
        assert_same_points(today.reachable_lattice_points(a, reg_a, from, ids, body_side, c.radius, c.spacing),
                           Cover::movement_range{}.reachable_lattice_points(cb, Pb.objects, from, ids, body_side,
                                                                            c.radius, c.spacing, orders));
        check_movement_range_orders(cb, orders, from, lattice_n, grid_order::grid_kind::object);
        arrayClear(orders);
        assert_same_points(today.reachable_lattice_points_without_critters(a, reg_a, from, body_side, c.radius,
                                                                           c.spacing),
                           read_only.reachable_lattice_points_without_critters(cb, Pb.objects, from, body_side,
                                                                               c.radius, c.spacing, orders));
        check_movement_range_orders(cb, orders, from, lattice_n, grid_order::grid_kind::pass);
        arrayClear(orders);
    }

    const auto cases = make_finder_cases();
    finder Fa, Fb;
    for (const auto& c : cases)
    {
        // Cases share chunks, so each gets cover grids of its own.
        Grid::GridRegistry reg_case;
        add_pools(reg_case);
        const auto P_case = pools_of(reg_case);
        auto& cover_case = reg_case.cover_pool(cover_params);
        const auto expected = find_today(Fa, a, reg_a, c);
        fm_assert(!expected.out_of_fills);
        const auto range = finder_chunks_of(c.q);
        const auto max_rounds = count_existing_chunks(cb, range) * 32;
        auto r = find_read_only(Fb, cb, P_case, c, orders);
        fm_assert(r.out_of_fills);
        fm_assert(!r.found);
        fm_assert_equal(0u, r.fills);
        const auto num_first_orders = (u32)orders.size();
        u32 num_rounds = 0;
        while (r.out_of_fills)
        {
            fm_assert(++num_rounds <= max_rounds);
            check_cover_orders(orders, range);
            run_cover_orders(b, cover_case, orders);
            arrayClear(orders);
            r = find_read_only(Fb, cb, P_case, c, orders);
            fm_assert_equal(0u, r.fills);
        }
        fm_assert(num_first_orders > 0);
        fm_assert(orders.isEmpty());
        assert_same_result(expected, r, c.list_spots);
    }

    for (const auto& s : los_segments)
    {
        const auto r = line_of_sight_today(a, reg_a, s);
        assert_same_ray(r, line_of_sight_read_only(cb, Pb, s, orders));
        fm_assert(!orders.isEmpty());
        check_pass_orders(orders, los_params);
        arrayClear(orders);
    }

    astar A;
    // The walk's path is found in a third world, so that a's grids are built only by the path check.
    auto tc = make_test_world();
    Grid::GridRegistry reg_c;
    const auto walk = make_walk(A, tc.w, reg_c, tc);
    for (u32 is_blocked = 0; is_blocked < 2; is_blocked++)
    {
        if (is_blocked)
        {
            add_blocker(a, walk.longest_leg_middle);
            add_blocker(b, walk.longest_leg_middle);
        }
        const auto expected_checks = check_path_today(walk.walk, a, reg_a, ta.critters.walker);
        const auto checks = check_path_read_only(walk.walk, cb, Pb, tb.critters.walker, orders, no_pool_orders);
        fm_assert_equal(expected_checks.with_pool, checks.with_pool);
        fm_assert_equal(expected_checks.without_pool, checks.without_pool);
        fm_assert_equal(!is_blocked, checks.with_pool);
        fm_assert(!orders.isEmpty());
        check_pass_orders(orders, walk_params);
        fm_assert(no_pool_orders.isEmpty());
        arrayClear(orders);
    }

    // Last, as today's can_move_to() makes the missing chunk in a.
    const auto movers = movers_of(ta);
    for (u32 i = 0; i < array_size(move_deltas); i++)
    {
        const auto can_move = a.find_object<critter>(movers[i])->can_move_to(move_deltas[i]);
        fm_assert_equal(can_move, cb.find_object<critter>(movers[i])->can_move_to(move_deltas[i]));
    }
    fm_assert(a.at(missing_chunk));
    fm_assert(!cb.at(missing_chunk));
}

} // namespace

void test_read_only_cover_queries()
{
    test_read_only_cover_writes_nothing();
    test_read_only_cover_with_built_grids();
    test_read_only_cover_without_grids();
}

} // namespace floormat::Test
