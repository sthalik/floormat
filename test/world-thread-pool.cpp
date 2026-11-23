#include "app.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-cover.hpp"
#include "src/old-rtree.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/thread-pool.hpp"
#include <array>

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

} // namespace

void test_world_thread_pool()
{
    test_world_without_pool();
    test_jobs_read_world();
    test_writes_outside_parallel_for();
}

} // namespace floormat::Test
