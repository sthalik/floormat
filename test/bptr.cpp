#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/assert.hpp"
#include "compat/defs.hpp"
#ifndef FM_NO_WEAK_BPTR
#include "compat/weak-borrowed-ptr.inl"
#endif
#include <array>
#include <cr/Debug.h>

namespace floormat {

namespace {
struct Foo : bptr_base
{
    int x;

    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Foo);
    Foo(int x) : x{x} {}
};
struct Bar : Foo {};
struct Baz : bptr_base {};
} // namespace

// NOLINTBEGIN(*-use-anonymous-namespace)

template class bptr<Foo>;
template class bptr<Bar>;
template class bptr<Baz>;

template bptr<Foo> static_pointer_cast(const bptr<Foo>&) noexcept;
template bptr<Bar> static_pointer_cast(const bptr<Bar>&) noexcept;
template bptr<Baz> static_pointer_cast(const bptr<Baz>&) noexcept;

template bptr<Bar> static_pointer_cast(const bptr<Foo>&) noexcept;
template bptr<Foo> static_pointer_cast(const bptr<Bar>&) noexcept;

//template bptr<Baz> static_pointer_cast(const bptr<Bar>&) noexcept; // must fail
//template bptr<Foo> static_pointer_cast(const bptr<Baz>&) noexcept; // must fail
//template bptr<Bar> static_pointer_cast(const bptr<Baz>&) noexcept; // must fail

// NOLINTEND(*-use-anonymous-namespace)

namespace {

int A_total = 0, A_alive = 0; // NOLINT

struct A : bptr_base
{
    int val, serial;
    explicit A(int val) : val{val}, serial{++A_total} { ++A_alive; }
    ~A() noexcept override { --A_alive; fm_assert(A_alive >= 0); }

    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(A);
};

void test1()
{
    A_total = 0; A_alive = 0;

    auto p1 = bptr<A>{InPlace, 1};
    fm_assert(p1.use_count() == 1);
    fm_assert(p1.get());
    fm_assert(A_total == 1);
    fm_assert(A_alive == 1);

    p1 = nullptr;
    fm_assert(p1.use_count() == 0);
    fm_assert(!p1.get());
    fm_assert(A_total == 1);
    fm_assert(A_alive == 0);
}

void test2()
{
    A_total = 0; A_alive = 0;

    auto p1 = bptr<A>{InPlace, 2};
    auto p2 = p1;

    fm_assert(p1.get());
    fm_assert(p1.get() == p2.get());
    fm_assert(p1->val == 2);
    fm_assert(p1->serial == 1);
    fm_assert(A_total == 1);
    fm_assert(A_alive == 1);

    p1 = nullptr;
    fm_assert(!p1.get());
    fm_assert(p2.get());
    fm_assert(p2->val == 2);
    fm_assert(p2->serial == 1);
    fm_assert(A_total == 1);
    fm_assert(A_alive == 1);

    p2 = nullptr;
    fm_assert(!p1.get());
    fm_assert(!p2.get());
    fm_assert(A_total == 1);
    fm_assert(A_alive == 0);
}

void test3()
{
    A_total = 0; A_alive = 0;

    auto p1 = bptr<A>{InPlace, 3};
    (void)p1;
    auto p2 = p1;
    auto p3 = p2;

    fm_assert(p1.use_count() == 3);
    fm_assert(p1.get());
    fm_assert(p1.get() == p2.get());
    fm_assert(p1.get() == p3.get());
    fm_assert(A_total == 1);
    fm_assert(A_alive == 1);

    p3 = nullptr; (void)p3;
    fm_assert(p1.use_count() == 2);
    p3 = p2;
    fm_assert(p1.use_count() == 3 && p3->val == 3 && p3->serial == 1 && p2 == p3 && p1 == p3);
}

void check_empty(const bptr<A>& p)
{
    fm_assert(!p);
    fm_assert(p.use_count() == 0);
    fm_assert(!p.get());
}

void check_nonempty(const bptr<A>& p)
{
    fm_assert(p);
    fm_assert(p.use_count() > 0);
    fm_assert(p.get());
}

void test4()
{
    A_total = 0; A_alive = 0;

    fm_assert(bptr<A>{} == bptr<A>{nullptr});
    fm_assert(bptr<A>{} == bptr<A>{(A*)nullptr});
    fm_assert(A_total == 0 && A_alive == 0);

    {
        auto p1 = bptr<A>{InPlace, 42};
        auto p2 = bptr<A>{InPlace, 41};
        auto p3 = bptr<A>{};
        check_empty(p3);
        (void)p1.operator=(p1);
        (void)p2.operator=(p2);
        fm_assert(p1->val == 42 && p1->serial == 1);
        fm_assert(p2->val == 41 && p2->serial == 2);
        fm_assert(A_total == 2);
        fm_assert(A_alive == 2);
        check_nonempty(p1);
        check_nonempty(p2);

        p1.swap(p2);
        fm_assert(p1->val == 41 && p1->serial == 2);
        fm_assert(p2->val == 42 && p2->serial == 1);
        fm_assert(A_total == 2);
        fm_assert(A_alive == 2);
        check_nonempty(p1);
        check_nonempty(p2);

        p1 = nullptr;
        fm_assert(A_total == 2);
        fm_assert(A_alive == 1);
        check_empty(p1);
        check_nonempty(p2);

        (void)p2;
        p1 = p2;
        fm_assert(p1 == p2);
        fm_assert(A_total == 2);
        fm_assert(A_alive == 1);
        check_nonempty(p1);
        check_nonempty(p2);

        p2 = bptr<A>{(A*)nullptr};
        check_empty(p2);
        check_nonempty(p1);
        fm_assert(A_total == 2);
        fm_assert(A_alive == 1);
        p1.reset();
        fm_assert(A_total == 2);
        fm_assert(A_alive == 0);

        p1 = p2;
        p2 = p1;
        fm_assert(A_total == 2);
        fm_assert(A_alive == 0);
    }
}

void test5()
{
    A_total = 0; A_alive = 0;
    auto p1 = bptr<A>{InPlace, -1};
    auto p2 = bptr<A>{InPlace, -2};
    fm_assert(A_total == 2);
    fm_assert(A_alive == 2);
    (void)p1;

    auto p3 = p1;
    fm_assert(p1.use_count() == 2);
    fm_assert(p2.use_count() == 1);
    fm_assert(A_total == 2);
    fm_assert(A_alive == 2);
    fm_assert(p1->serial == 1 && p1->val == -1);
    fm_assert(p2->serial == 2 && p2->val == -2);
    fm_assert(p3->serial == 1 && p3->val == -1);

    p1 = nullptr;
    fm_assert(!p1.get());
    fm_assert(p2->serial == 2 && p2->val == -2);
    fm_assert(p3->serial == 1 && p3->val == -1);
    fm_assert(A_total == 2);
    fm_assert(A_alive == 2);

    p2 = nullptr;
    fm_assert(!p1.get());
    fm_assert(!p2.get());
    fm_assert(p3->serial == 1 && p3->val == -1);
}

void test6()
{
    constexpr size_t size = 5;
    std::array<bptr<A>, size> array;
    for (auto n = 0u; n < size; n++)
    {
        A_total = 0; A_alive = 0;
        bptr<A> p0;
        array[0] = bptr<A>{InPlace, 6};
        for (auto i = 1u; i < size; i++)
            array[i] = array[i-1];
        fm_assert(array[0].use_count() == size);
        fm_assert(A_total == 1 && A_alive == 1);
        array[(n + 1) % size].reset();
        array[(n + 2) % size] = bptr<A>((A*)nullptr);
        array[(n + 3) % size] = move(p0);
        array[(n + 4) % size].swap(p0);
        array[(n + 0) % size] = nullptr;
        p0 = nullptr;
        for (auto k = 0u; k < size; k++)
        {
            for (auto i = 1u; i < size; i++)
                array[i-1] = array[(i+k) % size];
            for (auto i = 1u; i < size; i++)
                check_empty(array[i]);
        }
        fm_assert(A_total == 1);
        fm_assert(A_alive == 0);
        fm_assert(array == std::array<bptr<A>, size>{});
    }
}

void test7()
{
    A_total = 0; A_alive = 0;
    auto p1 = bptr<A>{InPlace, 7};
    auto p2 = bptr<A>{};
    p2 = move(p1);
    fm_assert(A_total == 1 && A_alive == 1);
    check_empty(p1);
    check_nonempty(p2);

    p1.reset();
    check_empty(p1);
    check_nonempty(p2);

    p1 = move(p2);
    fm_assert(A_total == 1 && A_alive == 1);
    check_nonempty(p1);
    check_empty(p2);

    p1 = move(p2);
    check_empty(p1);
    check_empty(p2);
    fm_assert(A_total == 1 && A_alive == 0);
}

void test8()
{
    A_total = 0; A_alive = 0;

    auto p1 = bptr<A>{InPlace, 81};
    auto p2 = bptr<A>{InPlace, 82};
    fm_assert(A_total == 2 && A_alive == 2);

    p1 = p2;
    fm_assert(A_total == 2 && A_alive == 1);

    p2 = move(p1); (void)p2;
    fm_assert(A_total == 2 && A_alive == 1);

    p1.reset();
    p2 = nullptr;
    (void)p2;
    fm_assert(A_total == 2 && A_alive == 0);
}

void test9()
{
    A_total = 0; A_alive = 0;

    auto p1 = bptr<A>{InPlace, 9};
    auto p2 = p1;
    p1 = p2;
    fm_assert(p1.use_count() == 2);
    fm_assert(A_total == 1);
    fm_assert(A_alive == 1);

    p1.destroy();
    fm_assert(!p1);
    fm_assert(!p2);
    fm_assert(p1.use_count() == 0);
    fm_assert(p2.use_count() == 0);
    fm_assert(A_total == 1);
    fm_assert(A_alive == 0);

    p1.swap(p2);
    fm_assert(!p1 && !p2);
    fm_assert(p1.use_count() == 0 && p2.use_count() == 0);
    fm_assert(A_total == 1);
    fm_assert(A_alive == 0);

    p1.reset();
    fm_assert(p1.use_count() == 0);
    fm_assert(p2.use_count() == 0);
    p2 = bptr<A>{(A*)nullptr};
    fm_assert(p1.use_count() == 0);
    fm_assert(p2.use_count() == 0);
    fm_assert(A_total == 1 && A_alive == 0);
}

void test10()
{
    static_assert(std::is_same_v<bptr<const Foo>, std::decay_t<decltype( bptr{std::declval<const Foo*>()} )>>);

    fm_assert(bptr<Foo>{} == bptr<Foo>{});
    fm_assert(bptr<const Foo>{} == bptr<const Foo>{});
    fm_assert(bptr<Foo>{} == bptr<const Foo>{});

    auto p1 = bptr<const Foo>{InPlace, 1}; (void)p1;
    //auto p2 = bptr<Foo>{p1};
    auto p3 = bptr<const Foo>{p1};      (void)p3;
    fm_assert(p1->x == 1); fm_assert(p3->x == 1);
    fm_assert(p1 == p3);

    auto p4 = bptr<Foo>{InPlace, 4};    (void)p4;
    auto p5 = bptr<const Foo>{p4};      (void)p5;
    //p4 = p5;
    fm_assert(p4->x == 4); fm_assert(p5->x == 4);
    fm_assert(p4 == p5);
    p5 = p4;
    fm_assert(p5->x == 4);
    fm_assert(p4 == p5);
    auto p6 = bptr<const Foo>{p5};      (void)p6;
    //p4.swap(p5);
    p5.swap(p6);
    fm_assert(p5 == p6);
    p6.destroy();
    fm_assert(!p6);
    fm_assert(p5 == p6);

    fm_assert(!bptr<const bptr_base>{p6});
    //fm_assert(bptr<bptr_base>{p6});
}

void test11()
{
    auto p1 = bptr<bptr_base>{new Foo{1}};
    auto p2 = static_pointer_cast<Foo>(p1);
    auto p3 = static_pointer_cast<bptr_base>(p1);

    fm_assert(p2->x == 1);
    fm_assert(p3);
    p1.destroy();
    fm_assert(!p2); fm_assert(!p3);

    p1.destroy();
    p1.destroy();
    p1.destroy();
    p2.destroy();
    p2.destroy();
    p2.destroy();
    p3.destroy();
    p3.destroy();
    p3.destroy();
    fm_assert(!p1); fm_assert(!p2); fm_assert(!p3);
}

void test12()
{
    auto p1 = bptr<bptr_base>{new Foo{1}};
    {
        fm_assert(p1.use_count() == 1);
        auto p2 = static_pointer_cast<Foo>(p1);
        fm_assert(p1.use_count() == 2);
    }
    fm_assert(p1.use_count() == 1);
}

void test13()
{
#ifndef FM_NO_WEAK_BPTR
    auto p1 = bptr<Foo>{InPlace, 13};
    fm_assert_equal(13, p1->x);
    auto w1 = weak_bptr{p1};
    fm_assert(p1); fm_assert(w1.lock());
    fm_assert_equal(1u, p1.use_count());
    auto p2 = p1;
    fm_assert_equal(2u, p2.use_count());
    p1 = {};
    fm_assert_equal(1u, p2.use_count());
    fm_assert(!p1); fm_assert(p2);
    fm_assert(w1.lock());
    fm_assert_equal(13, w1.lock()->x);
    p2 = {}; (void)p2;
    fm_assert(!w1.lock());
#endif
}

void test14()
{
#ifndef FM_NO_WEAK_BPTR
    auto p1 = bptr<Foo>{InPlace, 14};
    auto w1 = weak_bptr{p1};
    auto w2 = weak_bptr{p1};
    fm_assert_equal(14, w1.lock()->x);
    fm_assert_equal(14, w2.lock()->x);
    fm_assert_equal(1u, p1.use_count());
    w1 = {};
    fm_assert(p1);
    fm_assert_equal(1u, p1.use_count());
    w2 = {}; (void)w2;
    fm_assert(p1);
    fm_assert_equal(1u, p1.use_count());
    auto w3 = weak_bptr{p1};
    fm_assert_equal(14, w3.lock()->x);
    p1 = {}; (void)p1;
    fm_assert(!w1.lock()); fm_assert(!w2.lock()); fm_assert(!w3.lock());
#endif
}

} // namespace

void Test::test_bptr()
{
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
    test7();
    test8();
    test9();
    test10();
    test11();
    test12();
    test13();
    test14();
}

} // namespace floormat
