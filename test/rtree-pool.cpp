#include "app.hpp"
#include "compat/defs.hpp"
#include "src/rtree-pool.inl"
#include <cr/Debug.h>

namespace floormat::Test {

using detail::rtree_pool;

namespace {

struct Counter
{
    Counter() noexcept = default;
    ~Counter() noexcept { (*p)++; }

    fm_DECLARE_DEFAULT_COPY_ASSIGNMENT(Counter);

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
    size_t value;
    Counter counter;
    Liveness live{};
};

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
        fm_assert(!t2->live); fm_assert(t2->value != 37);
        fm_assert( t3->live); fm_assert(t3->value == 69);

        p.free(t1);
        fm_assert_equal(2, count);
        fm_assert_equal(1u, p.alive_count());
        fm_assert(!t1->live); fm_assert(t1->value != 42);
        fm_assert(!t2->live); fm_assert(t2->value != 37);
        fm_assert( t3->live); fm_assert(t3->value == 69);

        auto* const t4 = p.construct();
        fm_assert_equal(2, count);
        fm_assert_equal(2u, p.alive_count());
        fm_assert(t4 == t1);
        fm_assert( t1->live); fm_assert(t1->value != 42);
        fm_assert(!t2->live); fm_assert(t2->value != 37);
        fm_assert( t3->live); fm_assert(t3->value == 69);

        t4->counter.p = &count;
        t4->value = 667;
        fm_assert( t4->live);

        fm_assert_equal(2, count);
        fm_assert_equal(2u, p.alive_count());

        p.free(t3);
        fm_assert_equal(3, count);
        fm_assert_equal(1u, p.alive_count());
        fm_assert(!t2->live); fm_assert(t2->value != 37);
        fm_assert(!t3->live); fm_assert(t3->value != 69);
        fm_assert( t4->live); fm_assert(t3->value != 667);

        p.free(t4);
        fm_assert_equal(4, count);
        fm_assert_equal(0u, p.alive_count());
        fm_assert(!t2->live); fm_assert(t2->value != 37);
        fm_assert(!t3->live); fm_assert(t3->value != 69);
        fm_assert(!t4->live); fm_assert(t3->value != 667);
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

} // namespace

void test_rtree_pool()
{
    test1();
    test2();
}

} // namespace floormat::Test
