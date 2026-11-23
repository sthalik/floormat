#include "app.hpp"
#include "compat/defs.hpp"
#include "compat/thread-pool.hpp"
#include "src/rtree-pool.inl"
#include <cr/Array.h>
#include <cr/Debug.h>
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#endif

namespace floormat::Test {

using detail::rtree_pool;

namespace {

struct Counter
{
    Counter() noexcept = default;
    ~Counter() noexcept { (*p)++; }

    fm_DEFAULT_COPY(Counter);

    int* p = nullptr;
};

struct Liveness
{
    ~Liveness() noexcept { fm_assert(is_live); is_live = false; }
    explicit operator bool() const noexcept { return is_live; }

private:
    volatile bool is_live = true;
};

struct Tester
{
    Counter counter;
    u32 value;
    Liveness live{};
};
// rtree_pool::free() writes its free-list link over counter.p
static_assert(offsetof(Tester, counter) == 0);

bool is_dead(const Tester* t, const int* old_p)
{
#if fm_ASAN
    (void)old_p;
    return __asan_address_is_poisoned(t);
#else
    return !t->live && t->counter.p != old_p;
#endif
}

void test1()
{
    int count = 0;
    {
        rtree_pool<Tester> p;

        auto* const t1 = p.construct();
        t1->counter.p = &count;
        t1->value = 42;

        auto* const t2 = p.construct();
        t2->counter.p = &count;
        t2->value = 37;

        auto* const t3 = p.construct();
        t3->counter.p = &count;
        t3->value = 69;

        fm_assert_equal(0, count);
        fm_assert_equal(3u, p.alive_count());

        p.free(t2);
        fm_assert_equal(1, count);
        fm_assert_equal(2u, p.alive_count());

        fm_assert( t1->live); fm_assert(t1->value == 42);
        fm_assert(is_dead(t2, &count));
        fm_assert( t3->live); fm_assert(t3->value == 69);

        p.free(t1);
        fm_assert_equal(2, count);
        fm_assert_equal(1u, p.alive_count());
        fm_assert(is_dead(t1, &count));
        fm_assert(is_dead(t2, &count));
        fm_assert( t3->live); fm_assert(t3->value == 69);

        auto* const t4 = p.construct();
        fm_assert_equal(2, count);
        fm_assert_equal(2u, p.alive_count());
        fm_assert(t4 == t1);
        fm_assert( t1->live); fm_assert(t1->value != 42);
        fm_assert(is_dead(t2, &count));
        fm_assert( t3->live); fm_assert(t3->value == 69);

        t4->counter.p = &count;
        t4->value = 667;
        fm_assert( t4->live);

        fm_assert_equal(2, count);
        fm_assert_equal(2u, p.alive_count());

        p.free(t3);
        fm_assert_equal(3, count);
        fm_assert_equal(1u, p.alive_count());
        fm_assert(is_dead(t2, &count));
        fm_assert(is_dead(t3, &count));
        fm_assert( t4->live); fm_assert(t4->value == 667);

        p.free(t4);
        fm_assert_equal(4, count);
        fm_assert_equal(0u, p.alive_count());
        fm_assert(is_dead(t2, &count));
        fm_assert(is_dead(t3, &count));
        fm_assert(is_dead(t4, &count));
    }
    fm_assert_equal(4, count);
}

void test2()
{
    struct Tester {
        explicit Tester(const char*, int) {}
    };

    rtree_pool<Tester> P;
    fm_assert_equal(0zu, P.alive_count());

    auto* t1 = P.construct("foo", 42);
    auto* t2 = P.construct("bar", 69);
    fm_assert_equal(2zu, P.alive_count());

    P.free(t2);
    fm_assert_equal(1zu, P.alive_count());

    auto* t3 = P.construct("xyzzy", 37);
    fm_assert_equal(2zu, P.alive_count());
    fm_assert(t2 == t3);

    P.free(t1);
    P.free(t3);
    fm_assert_equal(0zu, P.alive_count());
}

constexpr u32 num_threads = 4, num_rounds = 100'000, num_kept_nodes = 8, num_nodes_before = 3;
constexpr u32 num_burst_nodes = 3000, num_rounds_between_bursts = 25'000;

struct node_contents
{
    u32 job, round, slot;
};

void check_contents(const node_contents* n, u32 job, u32 round, u32 slot)
{
    fm_assert(n->job == job && n->round == round && n->slot == slot);
}

// A burst holds more nodes than one batch allocation, so jobs refill the free list while others may use it.
void run_burst(rtree_pool<node_contents>& pool, u32 job, u32 round)
{
    auto burst = Array<node_contents*>{ValueInit, num_burst_nodes};
    for (auto i = 0u; i < num_burst_nodes; i++)
        burst[i] = pool.construct(job, round, i);
    for (auto i = 0u; i < num_burst_nodes; i++)
        check_contents(burst[i], job, round, i);
    for (auto i = 0u; i < num_burst_nodes; i++)
        pool.free(burst[i]);
}

void run_rounds(rtree_pool<node_contents>& pool, u32 job)
{
    node_contents* kept[num_kept_nodes] = {};
    u32 kept_rounds[num_kept_nodes] = {};
    u32 lcg_state = job;

    for (auto round = 0u; round < num_rounds; round++)
    {
        if (round % num_rounds_between_bursts == 0)
            run_burst(pool, job, round);

        lcg_state = lcg_state * 1664525 + 1013904223;
        const u32 i = (lcg_state >> 16) % num_kept_nodes;
        if (kept[i])
            pool.free(kept[i]);
        kept[i] = pool.construct(job, round, i);
        kept_rounds[i] = round;

        for (auto j = 0u; j < num_kept_nodes; j++)
            if (kept[j])
                check_contents(kept[j], job, kept_rounds[j], j);
    }

    for (auto* n : kept)
        if (n)
            pool.free(n);
}

void test_threads()
{
    rtree_pool<node_contents> pool;
    node_contents* before[num_nodes_before];
    for (auto i = 0u; i < num_nodes_before; i++)
        before[i] = pool.construct(num_threads, 0u, i);
    fm_assert_equal((usize)num_nodes_before, pool.alive_count());

    {
        ThreadPool thread_pool{num_threads};
        thread_pool.parallel_for(num_threads, 1, [&pool](u32 begin, u32 end, u32) {
            fm_assert(end == begin + 1);
            run_rounds(pool, begin);
        });
    }

    fm_assert_equal((usize)num_nodes_before, pool.alive_count());
    for (auto i = 0u; i < num_nodes_before; i++)
    {
        check_contents(before[i], num_threads, 0, i);
        pool.free(before[i]);
    }
    fm_assert_equal(0zu, pool.alive_count());
}

} // namespace

void test_rtree_pool()
{
    test1();
    test2();
    test_threads();
}

} // namespace floormat::Test
