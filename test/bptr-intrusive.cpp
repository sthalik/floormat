#include "app.hpp"
#include "bptr-common.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/weak-borrowed-ptr.inl"
#include "compat/exception.hpp"
#include <new>
#include <latch>
#include <thread>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>

// What doesn't work with intrusive_bptr_base. Each line fails to compile or aborts, so it stays
// commented out. IFoo derives from intrusive_bptr_base<P>.
//
// Compile errors:
//   new IFoo{1};  new IFoo[2];  new (std::nothrow) IFoo{1};  std::make_unique<IFoo>(1);
//       Class operator new is deleted. Only bptr{InPlace} puts the block in front of the object.
//   Pointer<IFoo>{InPlace, 1};  Array<IFoo>{ValueInit, 2};
//       They call new IFoo and new IFoo[]. Array's NoInit, DirectInit and InPlaceInit, and
//       the growable arrayAppend(), use placement new and compile.
//   basic_bptr<IFoo, other_policy>{InPlace, 1};  basic_bptr<IFoo, other_policy>{raw};
//       static_assert: an intrusive type has one policy, the one it derives with.
//   bptr{this}  in a type whose policy isn't non_atomic_refcount
//       bptr is basic_bptr<T, non_atomic_refcount>, so the same static_assert fires.
//       basic_bptr{this} deduces the type's own policy.
//   struct X : virtual intrusive_bptr_base<P> {};
//       dispose() can't static_cast down from a virtual base.
//   struct X : IFoo, IOther {};  with two intrusive bases
//       bptr_base is ambiguous.
//
// Aborts:
//   bptr{this} in the constructor, the base class constructors included
//       block_from_raw()'s fm_assert: the block's pointer stays odd until the constructor returns.
//   bptr{this} in the destructor, or bptr{raw} after destroy() while the block lives
//       block_from_raw()'s fm_assert: the block's pointer is null.
//   bptr{&stack_object};  bptr{&array[i]};  bptr{&*optional};  bptr{&member}
//       block_from_raw()'s fm_assert finds no block in front of the object. ASan may report the read first.
//   basic_bptr<bptr_base, P>{raw}
//       bptr_base isn't intrusive, so this makes a second, separate block.
//       Releasing it calls delete, and class operator delete aborts.
//   delete raw;  unique_ptr<IFoo>{raw}  and anything else that calls delete
//       Class operator delete aborts.
//   struct X : Other, intrusive_bptr_base<P> {};  with a polymorphic Other
//       Other goes to offset 0, so create()'s fm_assert fails on the first bptr{InPlace}.
//   bptr{raw} from another thread with thread_checked_refcount
//       The thread check, as with any copy.
//
// Undefined, as with any dangling pointer:
//   bptr{raw} after the last bptr and weak_bptr dropped reads freed memory. ASan reports it.

namespace floormat {

using namespace floormat::bptr_test;

namespace {

struct Plain : bptr_base { int x = 0; };
struct IPlain : intrusive_bptr_base<non_atomic_refcount> { int x = 0; };

template<typename T> concept can_new = requires { new T; };
template<typename T> concept can_new_array = requires { new T[2]; };
template<typename T> concept can_new_nothrow = requires { new (std::nothrow) T; };
template<typename T> concept can_placement_new = requires(void* p) { new (p) T; };

static_assert(can_new<Plain> && can_new_array<Plain> && can_new_nothrow<Plain> && can_placement_new<Plain>);
static_assert(!can_new<IPlain> && !can_new_array<IPlain> && !can_new_nothrow<IPlain>);
static_assert(can_placement_new<IPlain>);
static_assert(detail_bptr::Intrusive<IPlain> && !detail_bptr::Intrusive<Plain>);
static_assert(std::is_same_v<bptr<IPlain>, std::decay_t<decltype(bptr{std::declval<IPlain*>()})>>);
static_assert(std::is_same_v<bptr<IPlain>, decltype(basic_bptr{std::declval<IPlain*>()})>);
static_assert(std::is_same_v<bptr<Plain>, decltype(basic_bptr{std::declval<Plain*>()})>);

template<typename P>
struct suite
{
    template<typename T> using bptr = basic_bptr<T, P>;
    template<typename T> using weak_bptr = basic_weak_bptr<T, P>;
    static constexpr bool has_weak = P::has_weak;

    struct IFoo;
    struct IBar;
    struct IKill;
    struct INode;
    struct IWeakSelf;
    struct IThrow;
    struct IThrowLate;
    struct IAligned64;
    struct IAligned128;
    struct IBig;

    static int64_t allocations();
    static int64_t copies();

    static void test1();
    static void test2();
    static void test3();
    static void test4();
    static void test5();
    static void test6();
    static void test7();
    static void test8();
    static void test9();
    static void test10();
    static void test11();
    static void test12();
    static void test13();
    static void test14();
    static void test15();
    static void test16();
    static void test17();

    static void run(const char* policy);
};

template<typename P>
struct suite<P>::IFoo : intrusive_bptr_base<P>
{
    static inline int alive = 0;
    int x;

    explicit IFoo(int x) : x{x} { ++alive; }
    IFoo(const IFoo& other) noexcept : intrusive_bptr_base<P>{other}, x{other.x} { ++alive; }
    ~IFoo() noexcept override { --alive; fm_assert(alive >= 0); }

    bptr<IFoo> self() { return basic_bptr{this}; }
};

template<typename P>
struct suite<P>::IBar : IFoo
{
    int y;
    IBar(int x, int y) : IFoo{x}, y{y} {}
};

template<typename P>
struct suite<P>::IKill : intrusive_bptr_base<P>
{
    static inline int alive = 0;
    IKill() { ++alive; }
    ~IKill() noexcept override { --alive; fm_assert(alive >= 0); }

    // The object is gone when this returns, as after delete this.
    void kill() { bptr<IKill>{this}.destroy(); }
};

template<typename P>
struct suite<P>::INode : intrusive_bptr_base<P>
{
    static inline int alive = 0;
    bptr<INode> next;
    INode() { ++alive; }
    ~INode() noexcept override { --alive; fm_assert(alive >= 0); }
};

template<typename P>
struct suite<P>::IWeakSelf : intrusive_bptr_base<P>
{
    static inline int alive = 0;
    weak_bptr<IWeakSelf> me;
    IWeakSelf() { ++alive; }
    ~IWeakSelf() noexcept override
    {
        fm_assert(me.expired() && !me.lock());
        --alive;
        fm_assert(alive >= 0);
    }
};

template<typename P>
struct suite<P>::IThrow : intrusive_bptr_base<P>
{
    [[noreturn]] IThrow() { fm_throw("intrusive bptr ctor throws {}"_cf, 1); }
};

template<typename P>
struct suite<P>::IThrowLate : IFoo
{
    [[noreturn]] IThrowLate() : IFoo{2} { fm_throw("intrusive bptr ctor throws {}"_cf, 2); }
};

template<typename P>
struct alignas(64) suite<P>::IAligned64 : intrusive_bptr_base<P> { char data[64 - sizeof(void*)]{}; };

template<typename P>
struct alignas(128) suite<P>::IAligned128 : intrusive_bptr_base<P> { char data[128 - sizeof(void*)]{}; };

template<typename P>
struct suite<P>::IBig : intrusive_bptr_base<P> { char data[2048]{}; };

template<typename P>
int64_t suite<P>::allocations()
{
    if constexpr (counts_allocations<P>)
        return atomic_load(&P::allocator::total);
    else
        return 0;
}

template<typename P>
int64_t suite<P>::copies()
{
    if constexpr (has_stats<P>)
        return atomic_load(&P::stats::copies);
    else
        return 0;
}

template<typename P>
void suite<P>::test1()
{
    IFoo::alive = 0;
    auto before = heap_snapshot<P>();
    auto total = allocations();
    auto a = bptr<IFoo>{InPlace, 1};
    fm_assert(allocations() - total == int64_t{counts_allocations<P>});
    fm_assert(a && a->x == 1 && a.use_count() == 1 && IFoo::alive == 1);
    auto after = heap_snapshot<P>();
    fm_assert(after.blocks == before.blocks + has_stats<P> && after.objects == before.objects + has_stats<P>);
    a = nullptr;
    fm_assert(!a && IFoo::alive == 0);
    check_leaks<P>(before, "intrusive", "test1", 1);
}

template<typename P>
void suite<P>::test2()
{
    IFoo::alive = 0;
    auto a = bptr<IFoo>{InPlace, 2};
    IFoo* raw = &*a;
    auto before = heap_snapshot<P>();
    auto total = allocations();
    auto copied = copies();

    auto b = bptr<IFoo>{raw};
    fm_assert(b.get() == raw && b == a && a.use_count() == 2);
    auto c = raw->self();
    fm_assert(c == a && a.use_count() == 3);
    auto d = bptr<const IFoo>{static_cast<const IFoo*>(raw)};
    fm_assert(d == a && a.use_count() == 4);
    auto e = bptr<IFoo>{static_cast<IFoo*>(nullptr)};
    fm_assert(!e && !e.has_block() && a.use_count() == 4);

    fm_assert(allocations() == total);
    fm_assert(heap_snapshot<P>() == before);
    fm_assert(copies() - copied == 3 * has_stats<P>);

    a = nullptr;
    c = nullptr;
    fm_assert(IFoo::alive == 1 && b.use_count() == 2);
    b = nullptr;
    fm_assert(IFoo::alive == 1 && d.use_count() == 1 && d->x == 2);
    d = nullptr;
    fm_assert(IFoo::alive == 0);
}

// reset(raw) takes the new reference before it releases the old one, so resetting to the same
// object keeps it alive.
template<typename P>
void suite<P>::test3()
{
    IFoo::alive = 0;
    auto a = bptr<IFoo>{InPlace, 3}, c = bptr<IFoo>{InPlace, 5};
    a.reset(&*a);
    fm_assert(a && a->x == 3 && a.use_count() == 1 && IFoo::alive == 2);
    a = bptr<IFoo>{&*a};
    fm_assert(a && a.use_count() == 1 && IFoo::alive == 2);
    c.reset(&*a);
    fm_assert(c == a && a.use_count() == 2 && IFoo::alive == 1);
    c.reset();
    fm_assert(!c && a.use_count() == 1);
    a.reset(static_cast<IFoo*>(nullptr));
    fm_assert(!a && IFoo::alive == 0);
}

template<typename P>
void suite<P>::test4()
{
    IFoo::alive = 0;
    auto bar = bptr<IBar>{InPlace, 3, 7};
    auto foo = bptr<IFoo>{static_cast<IFoo*>(&*bar)};
    fm_assert(foo == bar && bar.use_count() == 2);
    auto bar2 = static_pointer_cast<IBar>(foo);
    fm_assert(bar2 == bar && bar2->y == 7 && bar.use_count() == 3);
    auto base = basic_bptr<intrusive_bptr_base<P>, P>{&*bar};
    fm_assert(base == bar && bar.use_count() == 4);
    // Converting an existing bptr works. bptr{raw} on a bptr_base* doesn't, see the top of the file.
    auto any = basic_bptr<bptr_base, P>{bar};
    fm_assert(any == bar && bar.use_count() == 5);
    auto foo2 = bar->self();
    fm_assert(foo2 == bar && bar.use_count() == 6);

    foo.destroy();
    fm_assert(IFoo::alive == 0);
    fm_assert(!bar && !foo && !bar2 && !base && !any && !foo2);
    fm_assert(bar.has_block() && any.has_block() && bar.use_count() == 0);
}

// destroy() runs the destructor at once. The other owners keep the block and read null.
template<typename P>
void suite<P>::test5()
{
    for (bool destroy_from_raw : {false, true})
    {
        IFoo::alive = 0;
        auto a = bptr<IFoo>{InPlace, 5};
        auto b = a->self();
        const IFoo* raw = a.get();
        auto before = heap_snapshot<P>();
        (destroy_from_raw ? b : a).destroy();
        auto after = heap_snapshot<P>();
        fm_assert(IFoo::alive == 0);
        fm_assert(!a && a.has_block() && !b && b.has_block());
        fm_assert(a == b && a == nullptr);
        fm_assert(poisoned(raw));
        fm_assert(after.objects == before.objects - has_stats<P>);
        fm_assert(after.blocks == before.blocks);
        fm_assert(after.allocations == before.allocations && after.bytes == before.bytes);
        a.destroy();
        fm_assert(IFoo::alive == 0);
        b = nullptr;
        fm_assert(heap_snapshot<P>().allocations == before.allocations);
        a = nullptr;
        after = heap_snapshot<P>();
        fm_assert(after.allocations == before.allocations - counts_allocations<P>);
        fm_assert(after.blocks == before.blocks - has_stats<P>);
    }
}

// The object destroys itself through a bptr{this} temporary, which leaves its owner null.
template<typename P>
void suite<P>::test6()
{
    IKill::alive = 0;
    auto a = bptr<IKill>{InPlace};
    auto b = a;
    a->kill();
    fm_assert(IKill::alive == 0 && !a && !b && a.has_block() && b.has_block());
}

template<typename P>
void suite<P>::test7()
{
    INode::alive = 0;

    auto a = bptr<INode>{InPlace};
    a->next = bptr<INode>{&*a};
    INode* raw = &*a;
    a = nullptr;
    fm_assert(INode::alive == 1);
    bptr<INode>{raw}.destroy();
    fm_assert(INode::alive == 0);

    auto x = bptr<INode>{InPlace}, y = bptr<INode>{InPlace}, z = bptr<INode>{InPlace};
    x->next = y;
    y->next = z;
    z->next = x;
    raw = &*y;
    x = y = z = nullptr;
    fm_assert(INode::alive == 3);
    bptr<INode>{raw}.destroy();
    fm_assert(INode::alive == 0);
}

template<typename P>
void suite<P>::test8()
{
    IFoo::alive = 0;
    auto a = bptr<IFoo>{InPlace, 8};
    auto w = weak_bptr<IFoo>{bptr<IFoo>{&*a}};
    fm_assert(!w.expired() && w.use_count() == 1);
    auto b = w.lock();
    fm_assert(b == a && a.use_count() == 2);
    weak_bptr<const IFoo> cw = w;
    fm_assert(cw == w && cw.lock() == a);
    b = nullptr;
    a = nullptr;
    fm_assert(IFoo::alive == 0 && w.expired() && !w.lock() && !cw.lock());
    fm_assert(w == weak_bptr<IFoo>{} && cw == w);
}

// A weak_bptr's lock() from inside the destructor sees the object as gone.
template<typename P>
void suite<P>::test9()
{
    IWeakSelf::alive = 0;
    auto a = bptr<IWeakSelf>{InPlace};
    a->me = a;
    a = nullptr;
    fm_assert(IWeakSelf::alive == 0);

    auto b = bptr<IWeakSelf>{InPlace};
    b->me = bptr<IWeakSelf>{&*b};
    auto c = b;
    b.destroy();
    fm_assert(IWeakSelf::alive == 0 && !c);
}

// A weak_bptr alone keeps the block and the object's storage after destroy(), as with make_shared.
template<typename P>
void suite<P>::test10()
{
    IFoo::alive = 0;
    auto a = bptr<IFoo>{InPlace, 10};
    auto w = weak_bptr<IFoo>{a};
    auto before = heap_snapshot<P>();
    a.destroy();
    fm_assert(IFoo::alive == 0 && w.expired() && !w.lock());
    a = nullptr;
    auto after = heap_snapshot<P>();
    fm_assert(after.allocations == before.allocations && after.blocks == before.blocks);
    w = nullptr;
    after = heap_snapshot<P>();
    fm_assert(after.allocations == before.allocations - counts_allocations<P>);
    fm_assert(after.blocks == before.blocks - has_stats<P>);
}

// Over-aligned objects get leading padding, and bptr{raw} still finds their block.
template<typename P>
void suite<P>::test11()
{
    auto aligned = [](const void* p, size_t n) { return (uintptr_t)p % n == 0; };
    auto a = bptr<IAligned64>{InPlace};
    auto b = bptr<IAligned128>{InPlace};
    fm_assert(aligned(a.get(), 64) && aligned(b.get(), 128));
    auto a2 = bptr<IAligned64>{&*a};
    auto b2 = bptr<IAligned128>{&*b};
    fm_assert(a2 == a && a.use_count() == 2 && b2 == b && b.use_count() == 2);
    b2.destroy();
    fm_assert(!b && b.has_block());
}

// Doesn't free anything at destroy(): the weak_bptr keeps all 2 KiB.
template<typename P>
void suite<P>::test12()
{
    auto total = allocations();
    auto a = bptr<IBig>{InPlace};
    fm_assert(allocations() - total == int64_t{counts_allocations<P>});
    if constexpr (has_weak)
    {
        auto w = weak_bptr<IBig>{a};
        auto before = heap_snapshot<P>();
        a.destroy();
        a = nullptr;
        auto after = heap_snapshot<P>();
        fm_assert(after.bytes == before.bytes && after.allocations == before.allocations);
        w = nullptr;
        fm_assert(heap_snapshot<P>().allocations == before.allocations - counts_allocations<P>);
    }
}

template<typename P>
void suite<P>::test13()
{
    IFoo::alive = 0;
    auto before = heap_snapshot<P>();
    int caught = 0;
    try {
        auto p = bptr<IThrow>{InPlace};
        (void)p;
    } catch (const floormat::exception&) {
        caught++;
    }
    try {
        auto p = bptr<IThrowLate>{InPlace};
        (void)p;
    } catch (const floormat::exception&) {
        caught++;
    }
    fm_assert(caught == 2 && IFoo::alive == 0);
    check_leaks<P>(before, "intrusive", "test13", 13);
}

// Copies outside bptr{InPlace} work as plain objects. bptr{raw} aborts on any of them.
template<typename P>
void suite<P>::test14()
{
    IFoo::alive = 0;
    {
        auto a = bptr<IFoo>{InPlace, 14};
        IFoo stack{1};
        IFoo copy{*a};
        Array<IFoo> arr;
        arrayReserve(arr, 1);
        arrayAppend(arr, InPlaceInit, 2);
        arrayAppend(arr, copy);
        arrayAppend(arr, InPlaceInit, 3);
        auto direct = Array<IFoo>{DirectInit, 2, 4};
        auto list = Array<IFoo>{InPlaceInit, {IFoo{5}, IFoo{6}}};
        Optional<IFoo> opt{InPlaceInit, 7};
        fm_assert(copy.x == 14 && arr[1].x == 14 && arr[2].x == 3 && direct[1].x == 4 && list[1].x == 6 && opt->x == 7);
        fm_assert(IFoo::alive == 1 + 2 + 3 + 2 + 2 + 1);
    }
    fm_assert(IFoo::alive == 0);
}

template<typename P>
void suite<P>::test15()
{
    IFoo::alive = 0;
    auto a = bptr<IFoo>{InPlace, 15};
    auto b = a->self();
    const void* first = a.get();
    a.destroy();
    fm_assert(poisoned(first));
    a = b = nullptr;
    auto c = bptr<IFoo>{InPlace, 16};
    if constexpr (pooling<P>)
        fm_assert(c.get() == first);
    auto d = c->self();
    fm_assert(d == c && c->x == 16 && c.use_count() == 2);
}

template<typename P>
void suite<P>::test16()
{
    constexpr int num_threads = 8, iterations = 10000;
    IFoo::alive = 0;
    auto a = bptr<IFoo>{InPlace, 16};
    IFoo* raw = &*a;
    std::latch start{num_threads};
    std::thread threads[num_threads];
    for (auto& t : threads)
        t = std::thread{[&] {
            start.arrive_and_wait();
            for (int i = 0; i < iterations; i++)
            {
                auto q = bptr<IFoo>{raw};
                fm_assert(q.get() == raw && q.use_count() >= 2);
            }
        }};
    for (auto& t : threads)
        t.join();
    fm_assert(a.use_count() == 1 && IFoo::alive == 1);
}

template<typename P>
void suite<P>::test17()
{
    IFoo::alive = 0;
    auto a = bptr<IBar>{InPlace, 17, 19};
    IBar* raw = &*a;
    auto b = basic_bptr{raw};
    auto c = basic_bptr{static_cast<const IFoo*>(raw)};
    auto d = raw->self();
    static_assert(std::is_same_v<decltype(b), bptr<IBar>>);
    static_assert(std::is_same_v<decltype(c), bptr<const IFoo>>);
    fm_assert(b == a && c == a && d == a && a.use_count() == 4);
    a = nullptr;
    b = nullptr;
    d = nullptr;
    fm_assert(IFoo::alive == 1 && c->x == 17);
    c = nullptr;
    fm_assert(IFoo::alive == 0);
}

template<typename P>
void suite<P>::run(const char* policy)
{
    run_test<P>(policy, 1, test1);
    run_test<P>(policy, 2, test2);
    run_test<P>(policy, 3, test3);
    run_test<P>(policy, 4, test4);
    run_test<P>(policy, 5, test5);
    run_test<P>(policy, 6, test6);
    run_test<P>(policy, 7, test7);
    if constexpr (has_weak)
    {
        run_test<P>(policy, 8, test8);
        run_test<P>(policy, 9, test9);
        run_test<P>(policy, 10, test10);
    }
    run_test<P>(policy, 11, test11);
    run_test<P>(policy, 12, test12);
    run_test<P>(policy, 13, test13);
    run_test<P>(policy, 14, test14);
    run_test<P>(policy, 15, test15);
    if constexpr (P::counter::concurrent)
    {
        auto before = heap_snapshot<P>(false);
        test16();
        check_leaks<P>(before, policy, "test", 16, false);
    }
    run_test<P>(policy, 17, test17);
}

} // namespace

void Test::test_bptr_intrusive()
{
    suite<non_atomic_refcount>::run("non_atomic");
    suite<atomic_refcount>::run("atomic");
    suite<thread_checked_refcount>::run("thread_checked");
    suite<counted_refcount>::run("counted");
    suite<small_refcount>::run("small");
    suite<pooled_refcount>::run("pooled");
    suite<strong_refcount>::run("strong");
    suite<atomic_counted_refcount>::run("atomic_counted");
}

} // namespace floormat
