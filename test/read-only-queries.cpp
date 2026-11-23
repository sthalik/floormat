#include "app.hpp"
#include "grid-read-only.hpp"
#include "src/grid-order.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-object.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/raycast.hpp"
#include "src/raycast-line.hpp"
#include "src/sweep-aabb.hpp"
#include "src/search-pred.hpp"
#include "src/point.inl"
#include "loader/loader.hpp"
#include "compat/array-size.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function-view.hpp"
#include <algorithm>
#include <bit>
#include <utility>
#include <cr/GrowableArray.h>

namespace floormat::Test {

namespace {

using Raycast::raycast_result_s;

constexpr chunk_coords_ COORD{0, 0, 0};
constexpr auto ray_params = Pass::Params{8, pass_mode::shoot_through};
constexpr auto box_params = Pass::Params{16, 32, pass_mode::pass};
constexpr auto box_size = Vector2ui{24};

struct query_pools
{
    Pass::Pool ray_pass{ray_params};
    Grid::Object::Pool ray_objects{ray_pass};
    Pass::Pool box_pass{box_params};
    Grid::Object::Pool box_objects{box_pass};
};

enum class pool_type : u8 { none, pass, object };

// px is relative to the center of COORD's first tile.
struct segment
{
    Vector2i from, to;
};

// The first four cross three chunks each. The fifth goes through add_colliders()' critter_id, at (992, 992).
constexpr segment segments[] = {
    {{-900, 100}, {1900, 700}},
    {{-950, -950}, {1950, 1950}},
    {{100, -900}, {300, 1900}},
    {{1900, -900}, {-900, 1900}},
    {{992, 700}, {992, 1300}},
    {{-500, 300}, {600, 300}},
    {{40, 600}, {1900, 640}},
};

struct query_case
{
    segment seg;
    Search::pred pred;
    pool_type pools;
    float radius;
};

point at(Vector2i px)
{
    return point::normalize_coords(point{COORD, {}, {}}, px);
}

struct box_line
{
    chunk_coords_ ch;
    Raycast::bresenham_line line;
};

box_line line_of(const segment& s)
{
    const auto from = at(s.from);
    const auto origin = point{from.chunk3(), {}, {}};
    return { from.chunk3(), Raycast::bresenham_line{from - origin, at(s.to) - origin} };
}

struct chunk_range
{
    chunk_coords_ lo, hi;
};

chunk_range chunks_of(const segment& s, i16 margin)
{
    const auto a = at(s.from).chunk3(), b = at(s.to).chunk3();
    return {
        { (i16)(std::min(a.x, b.x) - margin), (i16)(std::min(a.y, b.y) - margin), a.z },
        { (i16)(std::max(a.x, b.x) + margin), (i16)(std::max(a.y, b.y) + margin), a.z },
    };
}

Array<query_case> make_cases(const Search::pred& object_pred, const Search::pred& other_object_pred)
{
    Array<query_case> cases;
    arrayReserve(cases, array_size(segments) * 4);
    for (const auto& s : segments)
    {
        arrayAppend(cases, query_case{ s, object_pred, pool_type::object, 0 });
        arrayAppend(cases, query_case{ s, other_object_pred, pool_type::object, 2 });
        arrayAppend(cases, query_case{ s, Search::without_critters, pool_type::pass, 0 });
        arrayAppend(cases, query_case{ s, Search::without_critters, pool_type::none, 0 });
    }
    return cases;
}

u64 bits_of(collision_data x)
{
    return std::bit_cast<u64>(x);
}

raycast_result_s cast_ray(world& w, query_pools& P, const query_case& q)
{
    const auto from = at(q.seg.from), to = at(q.seg.to);
    if (q.pools == pool_type::object)
        return raycast(w, from, to, 0, P.ray_objects, q.pred, q.radius);
    else
        return raycast(w, from, to, 0, P.ray_pass, q.pred, q.radius);
}

raycast_result_s cast_ray(const world& w, const query_pools& P, const query_case& q, Array<grid_order>& orders)
{
    const auto from = at(q.seg.from), to = at(q.seg.to);
    if (q.pools == pool_type::object)
        return raycast(w, from, to, 0, P.ray_objects, q.pred, orders, q.radius);
    else
        return raycast(w, from, to, 0, P.ray_pass, q.pred, orders, q.radius);
}

swept_collider_result find_first_collider(world& w, query_pools& P, const query_case& q,
                                          std::array<Array<bool>, 4>& entry_blocked)
{
    const auto [ch, line] = line_of(q.seg);
    if (q.pools == pool_type::object)
        return find_swept_collider(w, ch, line, 0, line.steps(), box_size, q.pred, P.box_objects, entry_blocked);
    else
        return find_swept_collider(w, ch, line, 0, line.steps(), box_size, q.pred,
                                   q.pools == pool_type::pass ? &P.box_pass : nullptr);
}

swept_collider_result find_first_collider(const world& w, const query_pools& P, const query_case& q,
                                          Array<grid_order>& orders, std::array<Array<bool>, 4>& entry_blocked)
{
    const auto [ch, line] = line_of(q.seg);
    if (q.pools == pool_type::object)
        return find_swept_collider(w, ch, line, 0, line.steps(), box_size, q.pred, P.box_objects, orders,
                                   entry_blocked);
    else
        return find_swept_collider(w, ch, line, 0, line.steps(), box_size, q.pred, orders,
                                   q.pools == pool_type::pass ? &P.box_pass : nullptr);
}

void collect_swept_colliders(world& w, query_pools& P, const query_case& q,
                             std::array<Array<bool>, 4>& entry_blocked, Array<swept_collider_result>& out)
{
    const auto [ch, line] = line_of(q.seg);
    const auto f = [&](const swept_collider_result& r) {
        arrayAppend(out, r);
        return true;
    };
    if (q.pools == pool_type::object)
        find_swept_collider(w, ch, line, 0, line.steps(), box_size, q.pred, P.box_objects, entry_blocked, f);
    else
        find_swept_collider(w, ch, line, 0, line.steps(), box_size, q.pred,
                            q.pools == pool_type::pass ? &P.box_pass : nullptr, f);
}

void collect_swept_colliders(const world& w, const query_pools& P, const query_case& q, Array<grid_order>& orders,
                             std::array<Array<bool>, 4>& entry_blocked, Array<swept_collider_result>& out)
{
    const auto [ch, line] = line_of(q.seg);
    const auto f = [&](const swept_collider_result& r) {
        arrayAppend(out, r);
        return true;
    };
    if (q.pools == pool_type::object)
        find_swept_collider(w, ch, line, 0, line.steps(), box_size, q.pred, P.box_objects, orders, entry_blocked, f);
    else
        find_swept_collider(w, ch, line, 0, line.steps(), box_size, q.pred,
                            q.pools == pool_type::pass ? &P.box_pass : nullptr, orders, f);
}

// Sorts r by collider, then by step, and keeps each collider's first step. Which cells report a collider, and so
// how often, depends on which grids are built.
void keep_first_step_per_collider(Array<swept_collider_result>& r)
{
    std::sort(r.begin(), r.end(), [](const swept_collider_result& a, const swept_collider_result& b) {
        return bits_of(a.collider) != bits_of(b.collider) ? bits_of(a.collider) < bits_of(b.collider) : a.step < b.step;
    });
    u32 n = 0;
    for (u32 i = 0; i < (u32)r.size(); i++)
        if (!n || bits_of(r[i].collider) != bits_of(r[n-1].collider))
            r[n++] = r[i];
    arrayRemoveSuffix(r, r.size() - n);
}

void assert_same_ray(const raycast_result_s& a, const raycast_result_s& b)
{
    fm_assert_equal((bool)a.success, (bool)b.success);
    fm_assert_equal(bits_of(a.collider), bits_of(b.collider));
    fm_assert(a.collision == b.collision);
}

void assert_same_swept(const swept_collider_result& a, const swept_collider_result& b)
{
    fm_assert_equal(a.step, b.step);
    fm_assert_equal(a.has_collider, b.has_collider);
    fm_assert_equal(bits_of(a.collider), bits_of(b.collider));
    fm_assert(a.rect == b.rect);
}

void assert_same_swept(ArrayView<const swept_collider_result> a, ArrayView<const swept_collider_result> b)
{
    fm_assert_equal(a.size(), b.size());
    for (u32 i = 0; i < (u32)a.size(); i++)
        assert_same_swept(a[i], b[i]);
}

// Asserts that each order names a chunk in range, once, with the kind and the params of the pools given.
void check_orders(ArrayView<const grid_order> orders, const chunk_range& range, pool_type pools,
                  const Pass::Params& params)
{
    const auto kind = pools == pool_type::object ? grid_order::grid_kind::object : grid_order::grid_kind::pass;
    for (u32 i = 0; i < (u32)orders.size(); i++)
    {
        const auto& o = orders[i];
        fm_assert(o.kind == kind);
        fm_assert(o.pass_params == params);
        fm_assert_equal(range.lo.z, o.chunk.z);
        fm_assert(o.chunk.x >= range.lo.x && o.chunk.x <= range.hi.x);
        fm_assert(o.chunk.y >= range.lo.y && o.chunk.y <= range.hi.y);
        for (u32 j = 0; j < i; j++)
            fm_assert(orders[j].chunk != o.chunk);
    }
}

// Builds the grids that orders name, through P's pools.
void build_ordered_grids(world& w, query_pools& P, ArrayView<const grid_order> orders)
{
    for (const auto& o : orders)
    {
        auto& c = w[o.chunk];
        const bool is_ray_pool = o.pass_params == ray_params;
        fm_assert(is_ray_pool || o.pass_params == box_params);
        (is_ray_pool ? P.ray_pass : P.box_pass)[c].build_if_stale();
        if (o.kind == grid_order::grid_kind::object)
            (is_ray_pool ? P.ray_objects : P.box_objects)[c].build_if_stale();
    }
}

// Appends what a read-only query must leave unchanged: each chunk's stamps and flags, the grid each pool finds
// built for it with that grid's stamps, each pool's count of free grids, and world::pass_gen().
void record_state(const world& w, const query_pools& P, Array<u64>& out)
{
    for (i16 y = -2; y <= 2; y++)
        for (i16 x = -2; x <= 2; x++)
        {
            const auto* c = w.at(chunk_coords_{x, y, 0});
            arrayAppend(out, (u64)(c != nullptr));
            if (c)
            {
                arrayAppend(out, { c->pass_gen(), c->objects_gen(), (u64)c->is_passability_modified() });
                for (const auto* pool : { &P.ray_pass, &P.box_pass })
                {
                    const auto g = pool->find_built_grid(*c);
                    arrayAppend(out, { (u64)(bool)g, g ? g.build_no() : 0, g ? g.built_pass_gen() : 0 });
                }
                for (const auto* pool : { &P.ray_objects, &P.box_objects })
                {
                    const auto g = pool->find_built_grid(*c);
                    arrayAppend(out, { (u64)(bool)g, g ? g.built_pass_gen() : 0,
                                       g ? (u64)(uptr)g.entry_data().data() : 0 });
                }
            }
        }
    for (const auto* pool : { &P.ray_pass, &P.box_pass })
        arrayAppend(out, (u64)pool->free_grid_count());
    for (const auto* pool : { &P.ray_objects, &P.box_objects })
        arrayAppend(out, (u64)pool->free_grid_count());
    arrayAppend(out, w.pass_gen());
}

void assert_passability_clean(const world& w)
{
    for (i16 y = -1; y <= 1; y++)
        for (i16 x = -1; x <= 1; x++)
            fm_assert(!w.at(chunk_coords_{x, y, 0})->is_passability_modified());
}

// Test 2: on a world with chunks without grids, stale grids and critters, read-only calls change nothing.
void test_read_only_writes_nothing()
{
    auto w = make_world_with_ground();
    const auto [door_id, critter_id] = add_colliders(w);
    query_pools P;
    // built[3]'s grids go stale when its walls change, which also makes stale every grid of its neighbors built
    // before, so the others are built after.
    constexpr chunk_coords_ built[] = { {0, 0, 0}, {1, 0, 0}, {-1, -1, 0}, {0, 1, 0} };
    const auto build_grids = [&](chunk& c) {
        P.ray_pass[c].build_if_stale();
        P.ray_objects[c].build_if_stale();
        P.box_pass[c].build_if_stale();
        P.box_objects[c].build_if_stale();
    };
    build_grids(w[built[3]]);
    w[built[3]][local_coords{3, 3}].wall_north() = wall_image_proto{ loader.wall_atlas("empty"), 0 };
    rebuild_passability(w[built[3]]);
    for (u32 i = 0; i < 3; i++)
        build_grids(w[built[i]]);
    // A pass grid marked stale marks its neighbors' grids stale too.
    P.ray_pass[w[built[1]]].mark_stale();
    P.ray_objects[w[built[2]]].mark_stale();
    assert_passability_clean(w);
    const auto& cw = w;
    const auto& cP = P;
    fm_assert(cP.box_pass.find_built_grid(*cw.at(built[0])));
    fm_assert(cP.box_objects.find_built_grid(*cw.at(built[0])));
    fm_assert(!cP.ray_pass.find_built_grid(*cw.at(built[1])));
    fm_assert(!cP.ray_objects.find_built_grid(*cw.at(built[2])));
    fm_assert(!cP.ray_pass.find_built_grid(*cw.at(built[3])));
    fm_assert(!cP.ray_pass.find_built_grid(*cw.at(chunk_coords_{1, 1, 0})));

    Array<u64> before, after;
    record_state(cw, cP, before);
    const auto blocks_all_but_door = Search::blocks_all_but(door_id);
    const auto blocks_all_but_critter = Search::blocks_all_but(critter_id);
    const auto cases = make_cases(blocks_all_but_door, blocks_all_but_critter);
    std::array<Array<bool>, 4> entry_blocked;
    Array<grid_order> orders;
    Array<swept_collider_result> found;
    for (const auto& q : cases)
    {
        if (q.pools != pool_type::none)
            (void)cast_ray(cw, cP, q, orders);
        (void)find_first_collider(cw, cP, q, orders, entry_blocked);
        collect_swept_colliders(cw, cP, q, orders, entry_blocked, found);
    }
    fm_assert(!orders.isEmpty());
    record_state(cw, cP, after);
    fm_assert_equal(before.size(), after.size());
    for (u32 i = 0; i < (u32)before.size(); i++)
        fm_assert_equal(before[i], after[i]);
}

// Test 3: once today's overloads built the grids, read-only results equal theirs, with no order.
void test_read_only_with_built_grids()
{
    auto w = make_world_with_ground();
    const auto [door_id, critter_id] = add_colliders(w);
    const auto blocks_all_but_door = Search::blocks_all_but(door_id);
    const auto cases = make_cases(blocks_all_but_door, Search::never_continue);
    query_pools P;
    const auto& cw = w;
    const auto& cP = P;
    std::array<Array<bool>, 4> entry_blocked;
    Array<grid_order> orders;
    Array<swept_collider_result> expected, found;
    for (const auto& q : cases)
    {
        if (q.pools != pool_type::none)
        {
            const auto r = cast_ray(w, P, q);
            assert_same_ray(r, cast_ray(cw, cP, q, orders));
        }
        const auto box = find_first_collider(w, P, q, entry_blocked);
        assert_same_swept(box, find_first_collider(cw, cP, q, orders, entry_blocked));
        arrayClear(expected);
        arrayClear(found);
        collect_swept_colliders(w, P, q, entry_blocked, expected);
        collect_swept_colliders(cw, cP, q, orders, entry_blocked, found);
        assert_same_swept(expected, found);
        fm_assert(orders.isEmpty());
    }
}

struct today_results
{
    Array<raycast_result_s> rays;
    Array<swept_collider_result> boxes;
    Array<Array<swept_collider_result>> colliders;
};

// Runs the read-only calls of every case, compares them with expected and appends their orders to all_orders.
// Returns the number of rays and of boxes that the critter of id stopped.
u32 compare_read_only_results(const world& w, const query_pools& P, ArrayView<const query_case> cases,
                              const today_results& expected, object_id id, bool are_grids_missing,
                              Array<grid_order>& all_orders)
{
    std::array<Array<bool>, 4> entry_blocked;
    Array<grid_order> orders;
    Array<swept_collider_result> found;
    u32 num_stopped_by_critter = 0;
    for (u32 i = 0; i < (u32)cases.size(); i++)
    {
        const auto& q = cases[i];
        const auto check_call_orders = [&](const chunk_range& range, const Pass::Params& params) {
            check_orders(orders, range, q.pools, params);
            fm_assert_equal(are_grids_missing && q.pools != pool_type::none, !orders.isEmpty());
            arrayAppend(all_orders, orders);
            arrayClear(orders);
        };
        if (q.pools != pool_type::none)
        {
            const auto r = cast_ray(w, P, q, orders);
            assert_same_ray(expected.rays[i], r);
            num_stopped_by_critter += !r.success && r.collider.id == id;
            check_call_orders(chunks_of(q.seg, 0), ray_params);
        }
        const auto box = find_first_collider(w, P, q, orders, entry_blocked);
        assert_same_swept(expected.boxes[i], box);
        num_stopped_by_critter += box.has_collider && box.collider.id == id;
        check_call_orders(chunks_of(q.seg, 1), box_params);

        arrayClear(found);
        collect_swept_colliders(w, P, q, orders, entry_blocked, found);
        keep_first_step_per_collider(found);
        assert_same_swept(expected.colliders[i], found);
        check_call_orders(chunks_of(q.seg, 1), box_params);
    }
    return num_stopped_by_critter;
}

// Tests 4 and 5: without grids, read-only results equal today's on an identical copy, and every call with a pool
// orders grids. Once those are built, the same calls order nothing.
void test_read_only_without_grids()
{
    auto a = make_world_with_ground(), b = make_world_with_ground();
    const auto ids = add_colliders(a);
    const auto ids_b = add_colliders(b);
    fm_assert(ids.door_id == ids_b.door_id && ids.critter_id == ids_b.critter_id);
    const auto blocks_all_but_door = Search::blocks_all_but(ids.door_id);
    const auto cases = make_cases(blocks_all_but_door, Search::never_continue);
    query_pools Pa, Pb;
    const auto& cb = b;
    const auto& cPb = Pb;

    today_results expected = { .rays = Array<raycast_result_s>{ValueInit, cases.size()},
                               .boxes = Array<swept_collider_result>{ValueInit, cases.size()},
                               .colliders = Array<Array<swept_collider_result>>{ValueInit, cases.size()} };
    std::array<Array<bool>, 4> entry_blocked;
    for (u32 i = 0; i < (u32)cases.size(); i++)
    {
        const auto& q = cases[i];
        if (q.pools != pool_type::none)
            expected.rays[i] = cast_ray(a, Pa, q);
        expected.boxes[i] = find_first_collider(a, Pa, q, entry_blocked);
        collect_swept_colliders(a, Pa, q, entry_blocked, expected.colliders[i]);
        keep_first_step_per_collider(expected.colliders[i]);
    }

    Array<grid_order> all_orders;
    const auto num_stopped_by_critter = compare_read_only_results(cb, cPb, cases, expected, ids.critter_id, true,
                                                                  all_orders);
    fm_assert(num_stopped_by_critter >= 2);

    build_ordered_grids(b, Pb, all_orders);
    arrayClear(all_orders);
    (void)compare_read_only_results(cb, cPb, cases, expected, ids.critter_id, false, all_orders);
    fm_assert(all_orders.isEmpty());
}

// Test 6: with a pass pool only, a read-only ray and box on a chunk without a built grid see a critter in a cell
// no static collider blocks, which today's overloads miss.
void test_read_only_sees_critter_without_grid()
{
    auto a = make_world_with_ground(), b = make_world_with_ground();
    const auto id = add_critter_at(a, {600, 400}, 24, pass_mode::blocked)->id;
    fm_assert_equal(id, add_critter_at(b, {600, 400}, 24, pass_mode::blocked)->id);
    rebuild_passability(a[COORD]);
    rebuild_passability(b[COORD]);
    const auto blocks_critter = [id](const chunk&, collision_data x) {
        return x.id == id ? path_search_continue::blocked : path_search_continue::pass;
    };
    const auto q = query_case{ {{300, 400}, {900, 410}}, blocks_critter, pool_type::pass, 0 };
    query_pools Pa, Pb;
    const auto& cb = b;
    const auto& cPb = Pb;
    std::array<Array<bool>, 4> entry_blocked;
    Array<grid_order> orders;

    fm_assert(cast_ray(a, Pa, q).success);
    fm_assert(!find_first_collider(a, Pa, q, entry_blocked).has_collider);

    const auto r = cast_ray(cb, cPb, q, orders);
    fm_assert(!r.success);
    fm_assert_equal(id, (object_id)r.collider.id);
    const auto box = find_first_collider(cb, cPb, q, orders, entry_blocked);
    fm_assert(box.has_collider);
    fm_assert_equal(id, (object_id)box.collider.id);
    fm_assert(!orders.isEmpty());

    build_ordered_grids(b, Pb, orders);
    arrayClear(orders);
    fm_assert(cast_ray(cb, cPb, q, orders).success);
    fm_assert(!find_first_collider(cb, cPb, q, orders, entry_blocked).has_collider);
    fm_assert(orders.isEmpty());
}

// Test 7: one call of each of today's overloads, on a world without grids, builds the grids of the chunks it
// crosses, among them every chunk a read-only call on an identical copy orders.
void test_today_overloads_build()
{
    constexpr auto seg = segments[0];
    constexpr chunk_coords_ crossed[] = { {-1, 0, 0}, {0, 0, 0}, {1, 0, 0} };
    const auto copy = make_world_with_ground();
    std::array<Array<bool>, 4> entry_blocked;
    Array<grid_order> orders;
    Array<swept_collider_result> found;

    const auto check_built = [&](const world& w, const query_pools& P, pool_type pools, bool is_ray) {
        const auto& pass = is_ray ? P.ray_pass : P.box_pass;
        const auto& objects = is_ray ? P.ray_objects : P.box_objects;
        for (const auto& ch : crossed)
        {
            fm_assert(pass.find_built_grid(*w.at(ch)));
            fm_assert((pools == pool_type::object) == (bool)objects.find_built_grid(*w.at(ch)));
        }
        fm_assert(!orders.isEmpty());
        for (const auto& o : orders)
        {
            fm_assert(pass.find_built_grid(*w.at(o.chunk)));
            fm_assert(pools != pool_type::object || objects.find_built_grid(*w.at(o.chunk)));
        }
    };

    for (const auto pools : { pool_type::pass, pool_type::object })
    {
        const auto q = query_case{ seg, Search::never_continue, pools, 0 };
        const query_pools copy_pools;
        for (u32 call = 0; call < 3; call++)
        {
            auto w = make_world_with_ground();
            query_pools P;
            arrayClear(orders);
            arrayClear(found);
            if (call == 0)
            {
                (void)cast_ray(w, P, q);
                (void)cast_ray(copy, copy_pools, q, orders);
            }
            else if (call == 1)
            {
                (void)find_first_collider(w, P, q, entry_blocked);
                (void)find_first_collider(copy, copy_pools, q, orders, entry_blocked);
            }
            else
            {
                collect_swept_colliders(w, P, q, entry_blocked, found);
                collect_swept_colliders(copy, copy_pools, q, orders, entry_blocked, found);
            }
            check_built(w, P, pools, call == 0);
        }
    }
}

} // namespace

void test_read_only_queries()
{
    test_read_only_writes_nothing();
    test_read_only_with_built_grids();
    test_read_only_without_grids();
    test_read_only_sees_critter_without_grid();
    test_today_overloads_build();
}

} // namespace floormat::Test
