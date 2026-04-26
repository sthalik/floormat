#include "app.hpp"
#include "src/search-pool.hpp"
#include "src/grid-pass.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"

namespace floormat::Test {

namespace {

constexpr chunk_coords_ COORD{0, 0, 0};

void test_direct_hit_repeat()
{
    Pass::PoolRegistry r;
    auto& a = r.pool_for(64);
    auto& b = r.pool_for(64);
    fm_assert(&a == &b);
    fm_assert_equal(1u, r.live_pool_count());
}

void test_direct_miss_lazy_alloc()
{
    Pass::PoolRegistry r;
    fm_assert_equal(0u, r.live_pool_count());
    (void)r.pool_for(16);
    fm_assert_equal(1u, r.live_pool_count());
    (void)r.pool_for(32);
    fm_assert_equal(2u, r.live_pool_count());
    (void)r.pool_for(16);
    fm_assert_equal(2u, r.live_pool_count());
}

void test_odd_rounded()
{
    Pass::PoolRegistry r;
    auto& even = r.pool_for(64);
    auto& odd  = r.pool_for(63);
    fm_assert(&even == &odd);
    fm_assert_equal(1u, r.live_pool_count());
}

void test_dead_slot_routing()
{
    Pass::PoolRegistry r;
    auto& a = r.pool_for(2);
    auto& m = r.pool_for(4);
    fm_assert(&a == &m);
    fm_assert_equal(1u, r.live_pool_count());
    fm_assert(a.params().bbox_size == 4u);

    auto& six = r.pool_for(6);
    fm_assert(&six != &m);
    fm_assert(six.params().bbox_size == 6u);
    fm_assert_equal(2u, r.live_pool_count());
}

void test_zero_bbox()
{
    Pass::PoolRegistry r;
    auto& z = r.pool_for(0);
    auto& m = r.pool_for(4);
    fm_assert(&z == &m);
    fm_assert_equal(1u, r.live_pool_count());
    fm_assert(z.params().bbox_size == 4u);
}

void test_fallback_hit()
{
    Pass::PoolRegistry r;
    auto& a = r.pool_for(300);
    auto& b = r.pool_for(300);
    fm_assert(&a == &b);
    fm_assert_equal(1u, r.live_pool_count());
    fm_assert(a.params().bbox_size == 300u);
}

void test_fallback_distinct_bboxes()
{
    Pass::PoolRegistry r;
    auto& a = r.pool_for(300);
    auto& b = r.pool_for(500);
    fm_assert(&a != &b);
    fm_assert_equal(2u, r.live_pool_count());
    fm_assert(a.params().bbox_size == 300u);
    fm_assert(b.params().bbox_size == 500u);
    auto& aʹ = r.pool_for(300);
    auto& bʹ = r.pool_for(500);
    fm_assert(&a == &aʹ);
    fm_assert(&b == &bʹ);
    fm_assert_equal(2u, r.live_pool_count());
}

void test_overflow_at_max()
{
    Pass::PoolRegistry r;
    auto& a = r.pool_for(1024);
    fm_assert(a.params().bbox_size == 1024u);
    auto& b = r.pool_for(1024);
    fm_assert(&a == &b);
    fm_assert_equal(1u, r.live_pool_count());
}

void test_direct_and_fallback_coexist()
{
    Pass::PoolRegistry r;
    auto& d = r.pool_for(64);
    auto& f = r.pool_for(300);
    fm_assert(&d != &f);
    fm_assert_equal(2u, r.live_pool_count());
    fm_assert(d.params().bbox_size == 64u);
    fm_assert(f.params().bbox_size == 300u);
}

void test_frame_no_sync_touches_all_live()
{
    auto w = world();
    auto& c = w[COORD];
    (void)c;

    Pass::PoolRegistry r;
    auto& d = r.pool_for(64);
    auto& f = r.pool_for(300);
    (void)d; (void)f;
    fm_assert_equal(2u, r.live_pool_count());

    r.maybe_mark_stale_all(w.frame_no());
    r.build_if_stale_all();

    auto gd = d[c];
    auto gf = f[c];
    gd.build_if_stale();
    gf.build_if_stale();

    (void)w.increment_frame_no();
    r.maybe_mark_stale_all(w.frame_no());
    r.build_if_stale_all();

    auto gd2 = d[c];
    auto gf2 = f[c];
    gd2.build_if_stale();
    gf2.build_if_stale();
}

void test_max_direct_boundary()
{
    Pass::PoolRegistry r;
    auto& a = r.pool_for(254);
    fm_assert(a.params().bbox_size == 254u);
    auto& b = r.pool_for(256);
    fm_assert(&a != &b);
    fm_assert(b.params().bbox_size == 256u);
    fm_assert_equal(2u, r.live_pool_count());
}

} // namespace

void test_path_pool()
{
    test_direct_hit_repeat();
    test_direct_miss_lazy_alloc();
    test_odd_rounded();
    test_dead_slot_routing();
    test_zero_bbox();
    test_fallback_hit();
    test_fallback_distinct_bboxes();
    test_overflow_at_max();
    test_direct_and_fallback_coexist();
    test_max_direct_boundary();
    test_frame_no_sync_touches_all_live();
}

} // namespace floormat::Test
