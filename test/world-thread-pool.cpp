#include "app.hpp"
#include "order-list-access.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/grid-order.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-registry.hpp"
#include "src/order-list.inl"
#include "src/search-astar.hpp"
#include "src/old-rtree.hpp"
#include "compat/array-size.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/thread-pool.hpp"
#include <array>
#include <cr/GrowableArray.h>

namespace floormat::Test {

namespace {

constexpr u32 num_ranges = 64;
constexpr chunk_coords_ coord{0, 0, 0}, missing_coord{3, 0, 0};

struct job_reads
{
    const chunk* chunk_at_coord = nullptr;
    const chunk* chunk_at_missing_coord = nullptr;
    const critter* found_critter = nullptr;
    u64 max_pass_gen = 0, max_collider_gen = 0;
    int num_rtree_entries = 0;
    bool is_thread_pool_running = false, is_thread_pool_running_in_second_world = false;
};

bptr<critter> add_critter(world& w, local_coords pos)
{
    critter_proto p;
    p.name = "npc"_s;
    p.bbox_size = Vector2ub{24};
    return w.make_object<critter>(w.make_id(), global_coords{coord, pos}, move(p));
}

job_reads read_in_job(const world& w, const world& w2, object_id critter_id)
{
    const chunk* c = w.at(coord);
    return {
        .chunk_at_coord = c,
        .chunk_at_missing_coord = w.at(missing_coord),
        .found_critter = w.find_object<critter>(critter_id),
        .max_pass_gen = w.max_pass_gen(coord, 1),
        .max_collider_gen = w.max_collider_gen(coord, 1),
        .num_rtree_entries = c->rtree()->Count(),
        .is_thread_pool_running = w.is_thread_pool_running(),
        .is_thread_pool_running_in_second_world = w2.is_thread_pool_running(),
    };
}

std::array<job_reads, num_ranges> read_in_jobs(ThreadPool& pool, const world& w, const world& w2,
                                               object_id critter_id)
{
    std::array<job_reads, num_ranges> ret{};
    pool.parallel_for(num_ranges, 1, [&](u32 begin, u32 end, u32) {
        fm_assert(end == begin + 1);
        ret[begin] = read_in_job(w, w2, critter_id);
    });
    return ret;
}

void check_reads(const std::array<job_reads, num_ranges>& reads, const world& w, object_id critter_id,
                 bool is_thread_pool_running)
{
    const chunk* c = w.at(coord);
    fm_assert(c);
    for (const auto& r : reads)
    {
        fm_assert(r.chunk_at_coord == c);
        fm_assert(!r.chunk_at_missing_coord);
        fm_assert(r.found_critter && r.found_critter->id == critter_id);
        fm_assert(r.max_pass_gen == w.max_pass_gen(coord, 1));
        fm_assert(r.max_collider_gen == w.max_collider_gen(coord, 1));
        fm_assert(r.num_rtree_entries > 0 && r.num_rtree_entries == c->rtree()->Count());
        fm_assert(r.is_thread_pool_running == is_thread_pool_running);
        fm_assert(!r.is_thread_pool_running_in_second_world);
    }
}

void test_world_without_pool()
{
    world w;
    fm_assert(!w.thread_pool());
    fm_assert(!w.is_thread_pool_running());
}

void test_jobs_read_world()
{
    ThreadPool pool{3};
    world w, w2;
    auto& c = w[coord];
    const auto C = add_critter(w, {8, 8});
    (void)c.rtree();
    w.set_thread_pool(&pool);
    fm_assert(w.thread_pool() == &pool);
    fm_assert(!w.is_thread_pool_running());

    check_reads(read_in_jobs(pool, w, w2, C->id), w, C->id, true);
    fm_assert(!w.is_thread_pool_running());

    w.set_thread_pool(nullptr);
    fm_assert(!w.thread_pool());
    check_reads(read_in_jobs(pool, w, w2, C->id), w, C->id, false);
}

void test_writes_outside_parallel_for()
{
    ThreadPool pool{3};
    world w;
    auto& c = w[coord];
    w.set_thread_pool(&pool);
    pool.parallel_for(num_ranges, 1, [&](u32, u32, u32) {
        fm_assert(w.is_thread_pool_running());
    });

    fm_assert(!w.contains(missing_coord));
    (void)w[missing_coord];
    fm_assert(w.contains(missing_coord));

    const auto C = add_critter(w, {8, 8});
    auto i = C->index();
    const auto dest = global_coords{coord, {9, 8}};
    C->teleport_to(i, dest, {}, rotation_COUNT);
    fm_assert(C->coord == dest);

    Pass::Pool pass_pool{Pass::Params{16, pass_mode::pass}};
    auto g = pass_pool[c];
    fm_assert(g.is_marked_stale());
    g.build_if_stale();
    fm_assert(!g.is_marked_stale());

    Pass::Pool shoot_through_pool{Pass::Params{16, pass_mode::shoot_through}};
    Grid::Cover::Pool cover_pool{Grid::Cover::Params{16}, shoot_through_pool};
    auto cg = cover_pool[c];
    fm_assert(cg.ensure_octant(5) > 0);
    fm_assert(cg.built_octants() & 1u << 5);

    w.collect();
    fm_assert(!w.contains(missing_coord));
    fm_assert(w.contains(coord));
}

// Asserts that w has num_threads distinct think_context()s, the first naming frame_order_buffer() and
// grid_orders(). Each astar borrows astar_registry, or all are null when it is.
void check_think_contexts(world& w, u32 num_threads, Grid::GridRegistry* astar_registry)
{
    const auto buffers = w.order_buffers();
    fm_assert_equal(num_threads, (u32)buffers.size());
    const auto first = w.think_context(0);
    fm_assert(&first.orders == &w.frame_order_buffer());
    fm_assert(&first.grid_orders == &w.grid_orders());
    for (auto i = 0u; i < num_threads; i++)
    {
        const auto ctx = w.think_context(i);
        fm_assert(&ctx.orders == &buffers[i]);
        if (astar_registry)
            fm_assert(ctx.astar && &ctx.astar->pool_registry() == astar_registry);
        else
            fm_assert(!ctx.astar);
        for (auto j = 0u; j < i; j++)
        {
            const auto earlier = w.think_context(j);
            fm_assert(&ctx.grid_orders != &earlier.grid_orders);
            fm_assert(!astar_registry || ctx.astar != earlier.astar);
        }
    }
}

void test_think_contexts_without_pool()
{
    Grid::GridRegistry reg;
    world w;
    check_think_contexts(w, 1, nullptr);
    w.set_grid_registry(&reg);
    check_think_contexts(w, 1, nullptr);
}

void test_think_contexts_with_pool()
{
    ThreadPool pool{3};
    Grid::GridRegistry reg, other_reg;
    world w;
    w.set_thread_pool(&pool);
    check_think_contexts(w, 3, nullptr);
    w.set_grid_registry(&reg);
    check_think_contexts(w, 3, &reg);
    w.set_grid_registry(&other_reg);
    check_think_contexts(w, 3, &other_reg);
    w.set_grid_registry(nullptr);
    check_think_contexts(w, 3, nullptr);
    w.set_grid_registry(&reg);
    w.set_thread_pool(nullptr);
    check_think_contexts(w, 1, nullptr);
}

void test_thread_pool_change_keeps_capacity()
{
    constexpr u32 num_orders = 41;
    ThreadPool pool{3};
    world w;
    auto& buffer = w.frame_order_buffer();
    buffer.begin_object(0);
    for (auto i = 0u; i < num_orders; i++)
        buffer.add({});
    buffer.end_object();
    buffer.clear();
    auto& grid_orders = w.grid_orders();
    const auto o = grid_order{
        .pass_params = Pass::Params{16, pass_mode::pass}, .chunk = coord, .kind = grid_order::grid_kind::pass,
    };
    for (auto i = 0u; i < num_orders; i++)
        arrayAppend(grid_orders, o);
    arrayClear(grid_orders);
    const auto buffer_capacities = order_list_access::capacities(w.frame_order_buffer());
    const auto grid_order_capacity = arrayCapacity(w.grid_orders());
    fm_assert(buffer_capacities[0] >= num_orders && grid_order_capacity >= num_orders);
    for (auto* p : { &pool, (ThreadPool*)nullptr, &pool })
    {
        w.set_thread_pool(p);
        fm_assert(order_list_access::capacities(w.frame_order_buffer()) == buffer_capacities);
        fm_assert_equal(grid_order_capacity, arrayCapacity(w.grid_orders()));
    }
}

void test_thread_pool_change_keeps_grid_orders()
{
    ThreadPool pool{3};
    world w;
    const grid_order pending[] = {
        { .pass_params = Pass::Params{16, pass_mode::pass}, .chunk = coord, .kind = grid_order::grid_kind::pass,
          .update_index = 7 },
        { .pass_params = Pass::Params{16, pass_mode::pass}, .cover_div_size = 8, .chunk = missing_coord,
          .kind = grid_order::grid_kind::cover, .octant = 5, .update_index = 3 },
        { .pass_params = Pass::Params{32, pass_mode::shoot_through}, .chunk = coord,
          .kind = grid_order::grid_kind::object, .update_index = 11 },
    };
    for (const auto& o : pending)
        arrayAppend(w.grid_orders(), o);
    for (auto* p : { &pool, (ThreadPool*)nullptr })
    {
        w.set_thread_pool(p);
        const auto& orders = w.grid_orders();
        fm_assert_equal(array_size(pending), orders.size());
        for (auto i = 0u; i < array_size(pending); i++)
        {
            const auto &x = pending[i], &y = orders[i];
            fm_assert(x.kind == y.kind && x.chunk == y.chunk && x.update_index == y.update_index);
            fm_assert(x.pass_params == y.pass_params && x.cover_div_size == y.cover_div_size);
            fm_assert(x.octant == y.octant);
        }
    }
}

void test_moved_world_keeps_thread_arrays()
{
    ThreadPool pool{3};
    Grid::GridRegistry reg;
    world w1;
    fm_assert(w1.running_job_kind() == job_kind::make_critters_think);
    w1.set_thread_pool(&pool);
    w1.set_grid_registry(&reg);
    w1.set_running_job_kind(job_kind::rebuild_rtrees);
    world w2{move(w1)};
    fm_assert(w2.thread_pool() == &pool && w2.grid_registry() == &reg);
    fm_assert(w2.running_job_kind() == job_kind::rebuild_rtrees);
    fm_assert(w1.running_job_kind() == job_kind::make_critters_think);
    check_think_contexts(w2, 3, &reg);
    world w3;
    w3 = move(w2);
    fm_assert(w3.thread_pool() == &pool && w3.grid_registry() == &reg);
    fm_assert(w3.running_job_kind() == job_kind::rebuild_rtrees);
    fm_assert(w2.running_job_kind() == job_kind::make_critters_think);
    check_think_contexts(w3, 3, &reg);
}

} // namespace

void test_world_thread_pool()
{
    test_world_without_pool();
    test_jobs_read_world();
    test_writes_outside_parallel_for();
    test_think_contexts_without_pool();
    test_think_contexts_with_pool();
    test_thread_pool_change_keeps_capacity();
    test_thread_pool_change_keeps_grid_orders();
    test_moved_world_keeps_thread_arrays();
}

} // namespace floormat::Test
