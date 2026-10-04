#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/borrowed-ptr-atomic.hpp"
#include "compat/borrowed-ptr-stats.hpp"
#include "compat/weak-borrowed-ptr.inl"
#include "compat/assert.hpp"
#include "compat/atomic.hpp"
#include "compat/exception.hpp"
#include "compat/defs.hpp"
#include "random/random.hpp"
#include <array>
#include <initializer_list>
#include <latch>
#include <thread>
#include <cr/Debug.h>
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#if __has_include(<sanitizer/allocator_interface.h>)
#include <sanitizer/allocator_interface.h>
#endif
#endif

namespace floormat {

namespace {
struct Foo : bptr_base
{
    int x;

    fm_DISABLE_MOVE_COPY(Foo);
    Foo(int x) : x{x} {}
};
struct Bar : Foo { using Foo::Foo; };
struct Baz : bptr_base {};

template<typename Tag>
struct counting_allocator
{
    static inline volatile int64_t live, total;

    template<typename X> static void* allocate()
    {
        void* p = bptr_policy::new_delete_allocator::allocate<X>();
        atomic_fetch_add(&live, 1, memory_order::relaxed);
        atomic_fetch_add(&total, 1, memory_order::relaxed);
        return p;
    }
    template<typename X> static void deallocate(void* p) noexcept
    {
        atomic_fetch_sub(&live, 1, memory_order::relaxed);
        bptr_policy::new_delete_allocator::deallocate<X>(p);
    }
};

// Hands freed slots out again without telling ASan, as a real pool would.
template<typename Tag>
struct pool_allocator
{
    static constexpr bool pooling = true;
    static constexpr size_t slot_size = 256, slot_align = 64;
    struct slot { slot* next; };

    static inline slot* free_list = nullptr;
    static inline volatile int64_t live, total;

    template<typename X> static void* allocate()
    {
        void* p;
        if constexpr (sizeof(X) > slot_size || alignof(X) > slot_align)
            p = bptr_policy::new_delete_allocator::allocate<X>();
        else if (free_list)
        {
            p = free_list;
            free_list = free_list->next;
        }
        else
            p = ::operator new(slot_size, std::align_val_t{slot_align});
        atomic_fetch_add(&live, 1);
        atomic_fetch_add(&total, 1);
        return p;
    }
    template<typename X> static void deallocate(void* p) noexcept
    {
        atomic_fetch_sub(&live, 1);
        if constexpr (sizeof(X) > slot_size || alignof(X) > slot_align)
            bptr_policy::new_delete_allocator::deallocate<X>(p);
        else
            free_list = ::new (p) slot{free_list};
    }
    static void drain() noexcept
    {
        while (auto* s = free_list)
        {
            free_list = s->next;
            ::operator delete(s, std::align_val_t{slot_align});
        }
    }
};

struct counted_refcount : non_atomic_refcount
{
    using allocator = counting_allocator<counted_refcount>;
    using stats = bptr_policy::counting_stats<counted_refcount>;
};

struct small_refcount : non_atomic_refcount
{
    using counter = bptr_policy::non_atomic_counter<uint16_t>;
    using allocator = counting_allocator<small_refcount>;
    using stats = bptr_policy::counting_stats<small_refcount>;
};

struct pooled_refcount : non_atomic_refcount
{
    using allocator = pool_allocator<pooled_refcount>;
    using stats = bptr_policy::counting_stats<pooled_refcount>;
};

struct strong_refcount : non_atomic_refcount
{
    using allocator = counting_allocator<strong_refcount>;
    using stats = bptr_policy::counting_stats<strong_refcount>;
    static constexpr bool has_weak = false;
};

struct atomic_counted_refcount : atomic_refcount
{
    using allocator = counting_allocator<atomic_counted_refcount>;
    using stats = bptr_policy::counting_stats<atomic_counted_refcount>;
};

struct thread_counted_refcount : thread_checked_refcount
{
    using allocator = counting_allocator<thread_counted_refcount>;
    using stats = bptr_policy::counting_stats<thread_counted_refcount>;
};

int Custom_disposed = 0; // NOLINT

struct Custom : bptr_base
{
    int x;
    explicit Custom(int x) : x{x} {}
};

// Allocates with plain new, so the test can tell it apart from detail_bptr::ptr_block.
struct custom_ptr_block final : detail_bptr::control_block<counted_refcount>
{
    using control_block::control_block;

    static control_block* create(bptr_base* p) noexcept { return new custom_ptr_block{p}; }

    void dispose(bptr_base* p) noexcept override
    {
        ++Custom_disposed;
        delete p;
    }

    void deallocate() noexcept override { delete this; }
};
} // namespace

template<>
struct bptr_traits<Custom, counted_refcount> : default_bptr_traits<Custom, counted_refcount>
{
    using ptr_block = custom_ptr_block;
};

// NOLINTBEGIN(*-use-anonymous-namespace)

template class basic_bptr<Foo, non_atomic_refcount>;
template class basic_bptr<Bar, non_atomic_refcount>;
template class basic_bptr<Baz, non_atomic_refcount>;
template class basic_bptr<Foo, atomic_refcount>;
template class basic_bptr<Foo, thread_checked_refcount>;
template class basic_bptr<Foo, small_refcount>;
template class basic_bptr<Foo, strong_refcount>;
template class basic_weak_bptr<Foo, non_atomic_refcount>;
template class basic_weak_bptr<Foo, atomic_refcount>;

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

static_assert(std::is_same_v<bptr<const Foo>, std::decay_t<decltype( bptr{std::declval<const Foo*>()} )>>);
static_assert(std::is_same_v<weak_bptr<Foo>, decltype(weak_bptr{std::declval<const bptr<Foo>&>()})>);
static_assert(std::is_same_v<basic_weak_bptr<Foo, atomic_refcount>,
                             decltype(basic_weak_bptr{std::declval<const basic_bptr<Foo, atomic_refcount>&>()})>);

template<typename To, typename From> concept can_cast = requires(From p) { static_pointer_cast<To>(p); };
template<typename To, typename From> concept can_cast_rv = requires(From p) { static_pointer_cast<To>(move(p)); };
template<typename X, typename Y> concept can_compare = requires(const X& x, const Y& y) { x == y; y == x; x != y; };

static_assert(std::is_constructible_v<bptr<const Foo>, const bptr<Foo>&>);
static_assert(std::is_constructible_v<bptr<const Foo>, bptr<Foo>&&>);
static_assert(std::is_assignable_v<bptr<const Foo>&, const bptr<Foo>&>);
static_assert(std::is_constructible_v<bptr<const Foo>, const bptr<Bar>&>);
static_assert(std::is_constructible_v<weak_bptr<const Foo>, const weak_bptr<Foo>&>);
static_assert(std::is_constructible_v<weak_bptr<const Foo>, weak_bptr<Foo>&&>);
static_assert(std::is_assignable_v<weak_bptr<const Foo>&, const weak_bptr<Foo>&>);
static_assert(std::is_constructible_v<weak_bptr<const Foo>, const bptr<Foo>&>);
static_assert(std::is_constructible_v<weak_bptr<const Foo>, const weak_bptr<Bar>&>);

static_assert(!std::is_constructible_v<bptr<Foo>, const bptr<const Foo>&>);
static_assert(!std::is_constructible_v<bptr<Foo>, bptr<const Foo>&&>);
static_assert(!std::is_assignable_v<bptr<Foo>&, const bptr<const Foo>&>);
static_assert(!std::is_assignable_v<bptr<Foo>&, bptr<const Foo>&&>);
static_assert(!std::is_constructible_v<weak_bptr<Foo>, const weak_bptr<const Foo>&>);
static_assert(!std::is_constructible_v<weak_bptr<Foo>, weak_bptr<const Foo>&&>);
static_assert(!std::is_assignable_v<weak_bptr<Foo>&, const weak_bptr<const Foo>&>);
static_assert(!std::is_assignable_v<weak_bptr<Foo>&, weak_bptr<const Foo>&&>);
static_assert(!std::is_constructible_v<weak_bptr<Foo>, const bptr<const Foo>&>);
static_assert(!std::is_assignable_v<weak_bptr<Foo>&, const bptr<const Foo>&>);

static_assert(!std::is_constructible_v<bptr<Bar>, const bptr<Foo>&>);
static_assert(!std::is_constructible_v<weak_bptr<Bar>, const weak_bptr<Foo>&>);

static_assert(can_cast<Bar, bptr<Foo>> && can_cast_rv<Bar, bptr<Foo>>);
static_assert(can_cast<const Bar, bptr<const Foo>> && can_cast_rv<const Bar, bptr<const Foo>>);
static_assert(!can_cast<Foo, bptr<const Foo>> && !can_cast_rv<Foo, bptr<const Foo>>);
static_assert(!can_cast<Bar, bptr<const Foo>> && !can_cast_rv<Bar, bptr<const Foo>>);

static_assert(can_compare<bptr<Foo>, bptr<Bar>> && can_compare<bptr<const Foo>, bptr<Bar>>);
static_assert(can_compare<weak_bptr<Foo>, weak_bptr<Bar>> && can_compare<weak_bptr<const Foo>, weak_bptr<Bar>>);
static_assert(!can_compare<bptr<Foo>, bptr<Baz>> && !can_compare<weak_bptr<Foo>, weak_bptr<Baz>>);

static_assert(sizeof(void*) != 8 || sizeof(detail_bptr::ptr_block<non_atomic_refcount>) == 24);
static_assert(sizeof(void*) != 8 || sizeof(detail_bptr::ptr_block<atomic_refcount>) == 24);
static_assert(sizeof(void*) != 8 || sizeof(detail_bptr::ptr_block<thread_checked_refcount>) == 32);
static_assert(sizeof(detail_bptr::inplace_block<Foo, non_atomic_refcount>) ==
              sizeof(detail_bptr::ptr_block<non_atomic_refcount>) + sizeof(Foo));

// Only ASan exposes the heap total, so elsewhere a leaked control block goes unnoticed.
size_t allocated_bytes()
{
#if fm_ASAN && __has_include(<sanitizer/allocator_interface.h>)
    return __sanitizer_get_current_allocated_bytes();
#else
    return 0;
#endif
}

bool poisoned(const void* p)
{
#if fm_ASAN
    return __asan_address_is_poisoned(p);
#else
    (void)p;
    return true;
#endif
}

template<typename P> constexpr bool has_stats = requires { P::stats::live_blocks; };
template<typename P> constexpr bool counts_allocations = requires { P::allocator::live; };
template<typename P> constexpr bool pooling = requires { requires P::allocator::pooling; };

struct heap_state
{
    size_t bytes = 0;
    int64_t blocks = 0, objects = 0, allocations = 0;
    bool operator==(const heap_state&) const = default;
};

// Thread start-up allocates outside the test's control, so thread tests skip the heap total.
template<typename P>
heap_state heap_snapshot(bool bytes = true)
{
    heap_state s;
    if constexpr (pooling<P>)
        P::allocator::drain();
    if (bytes)
        s.bytes = allocated_bytes();
    if constexpr (has_stats<P>)
    {
        s.blocks = atomic_load(&P::stats::live_blocks);
        s.objects = atomic_load(&P::stats::live_objects);
    }
    if constexpr (counts_allocations<P>)
        s.allocations = atomic_load(&P::allocator::live);
    return s;
}

template<typename P>
void check_leaks(const heap_state& before, const char* policy, const char* what, uint32_t n, bool bytes = true)
{
    auto after = heap_snapshot<P>(bytes);
    if (after != before)
        fm_abort("bptr %s %s %u: leaked %lld bytes, %lld blocks, %lld objects, %lld allocations",
                 policy, what, n, (long long)(after.bytes - before.bytes), (long long)(after.blocks - before.blocks),
                 (long long)(after.objects - before.objects), (long long)(after.allocations - before.allocations));
}

template<typename P>
void run_test(const char* policy, uint32_t n, void(*test)())
{
    auto before = heap_snapshot<P>();
    test();
    check_leaks<P>(before, policy, "test", n);
}

int A_total = 0, A_alive = 0; // NOLINT

struct A : bptr_base
{
    int val, serial;
    explicit A(int val) : val{val}, serial{++A_total} { ++A_alive; }
    ~A() noexcept override { --A_alive; fm_assert(A_alive >= 0); }

    fm_DISABLE_MOVE_COPY(A);
};

int Node_alive = 0; // NOLINT

template<typename P>
struct suite
{
    template<typename T> using bptr = basic_bptr<T, P>;
    template<typename T> using weak_bptr = basic_weak_bptr<T, P>;
    static constexpr bool has_weak = P::has_weak;

    struct Node;
    struct Probe;

    static void check_empty(const bptr<A>& p);
    static void check_nonempty(const bptr<A>& p);
    static Node* make_self_owned();

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
    static void test18();
    static void test19();
    static void test20();
    static void test21();
    static void test22();
    static void test23();
    static void test25();
    static void test26();
    static void test27();
    static void test31();

    static void run(const char* policy);
};

template<typename P>
void suite<P>::test1()
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

template<typename P>
void suite<P>::test2()
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

template<typename P>
void suite<P>::test3()
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

template<typename P>
void suite<P>::check_empty(const bptr<A>& p)
{
    fm_assert(!p);
    fm_assert(p.use_count() == 0);
    fm_assert(!p.get());
    fm_assert(p == bptr<A>{});
}

template<typename P>
void suite<P>::check_nonempty(const bptr<A>& p)
{
    fm_assert(p);
    fm_assert(p.use_count() > 0);
    fm_assert(p.get());
}

template<typename P>
void suite<P>::test4()
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

template<typename P>
void suite<P>::test5()
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

template<typename P>
void suite<P>::test6()
{
    constexpr size_t size = 5;
    std::array<bptr<A>, size> array;
    for (auto n = 0u; n < size; n++)
    {
        A_total = 0; A_alive = 0;
        bptr<A> p0;
        array[0] = bptr<A>{InPlace, 6};
        fm_assert(array[0].use_count() == 1);
        for (auto i = 1u; i < size; i++)
        {
            array[i] = array[i-1];
            fm_assert(array[0].use_count() == i+1);
            fm_assert(array[i] == array[0]);
        }
        fm_assert(array[0].use_count() == size);
        fm_assert(A_total == 1 && A_alive == 1);

        array[(n + 1) % size].reset();
        check_empty(array[(n + 1) % size]);
        fm_assert(array[(n + 0) % size].use_count() == 4);
        fm_assert(A_alive == 1);

        array[(n + 2) % size] = bptr<A>((A*)nullptr);
        check_empty(array[(n + 2) % size]);
        fm_assert(array[(n + 0) % size].use_count() == 3);
        fm_assert(A_alive == 1);

        array[(n + 3) % size] = move(p0);
        check_empty(array[(n + 3) % size]);
        check_empty(p0);
        fm_assert(array[(n + 0) % size].use_count() == 2);
        fm_assert(A_alive == 1);

        array[(n + 4) % size].swap(p0);
        check_empty(array[(n + 4) % size]);
        fm_assert(p0 == array[(n + 0) % size]);
        fm_assert(p0.use_count() == 2);
        fm_assert(A_alive == 1);

        array[(n + 0) % size] = nullptr;
        check_empty(array[(n + 0) % size]);
        fm_assert(p0.use_count() == 1);
        fm_assert(p0->val == 6 && A_alive == 1);

        p0 = nullptr;
        check_empty(p0);
        fm_assert(A_alive == 0);

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

template<typename P>
void suite<P>::test7()
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

template<typename P>
void suite<P>::test8()
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

template<typename P>
void suite<P>::test9()
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

template<typename P>
void suite<P>::test10()
{
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

template<typename P>
void suite<P>::test11()
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

template<typename P>
void suite<P>::test12()
{
    auto p1 = bptr<bptr_base>{new Foo{1}};
    {
        fm_assert(p1.use_count() == 1);
        auto p2 = static_pointer_cast<Foo>(p1);
        fm_assert(p1.use_count() == 2);
    }
    fm_assert(p1.use_count() == 1);
}

template<typename P>
void suite<P>::test13()
{
    auto p1 = bptr<Foo>{InPlace, 13};
    fm_assert_equal(13, p1->x);
    auto w1 = basic_weak_bptr{p1};
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
}

template<typename P>
void suite<P>::test14()
{
    auto p1 = bptr<Foo>{InPlace, 14};
    auto w1 = basic_weak_bptr{p1};
    auto w2 = basic_weak_bptr{p1};
    fm_assert_equal(14, w1.lock()->x);
    fm_assert_equal(14, w2.lock()->x);
    fm_assert_equal(1u, p1.use_count());
    w1 = {};
    fm_assert(p1);
    fm_assert_equal(1u, p1.use_count());
    w2 = {}; (void)w2;
    fm_assert(p1);
    fm_assert_equal(1u, p1.use_count());
    auto w3 = basic_weak_bptr{p1};
    fm_assert_equal(14, w3.lock()->x);
    p1 = {}; (void)p1;
    fm_assert(!w1.lock()); fm_assert(!w2.lock()); fm_assert(!w3.lock());
}

struct throwing_ctor : bptr_base
{
    [[noreturn]] throwing_ctor() { fm_throw("bptr in-place ctor must propagate exception {}"_cf, 15); }
};

// regression: a throwing T ctor inside bptr{InPlace} must propagate a catchable
// exception rather than terminate (was terminate under unconditional noexcept)
template<typename P>
void suite<P>::test15()
{
    bool caught = false;
    try {
        auto p = bptr<throwing_ctor>{InPlace};
        (void)p;
    } catch (const floormat::exception&) {
        caught = true;
    }
    fm_assert(caught);
}

template<typename P>
struct suite<P>::Node : bptr_base
{
    int val;
    bptr<Node> next;

    Node(int val, bptr<Node> next) : val{val}, next{move(next)} { ++Node_alive; }
    ~Node() noexcept override { --Node_alive; fm_assert(Node_alive >= 0); }

    fm_DISABLE_MOVE_COPY(Node);
};

// the source lives inside the object the assignment releases
template<typename P>
void suite<P>::test16()
{
    Node_alive = 0;
    auto p = bptr<Node>{InPlace, 1, bptr<Node>{InPlace, 2, bptr<Node>{InPlace, 3, bptr<Node>{}}}};
    fm_assert(Node_alive == 3);

    auto& q = p;
    p = move(q);
    fm_assert(Node_alive == 3);
    fm_assert(p->val == 1 && p.use_count() == 1);

    p = p->next;
    fm_assert(Node_alive == 2);
    fm_assert(p->val == 2 && p.use_count() == 1);
    fm_assert(p->next->val == 3 && p->next.use_count() == 1);

    p = move(p->next);
    fm_assert(Node_alive == 1);
    fm_assert(p->val == 3 && p.use_count() == 1);
    fm_assert(!p->next);

    p = p->next;
    fm_assert(Node_alive == 0);
    fm_assert(!p && p == bptr<Node>{});
}

template<typename P>
void suite<P>::test17()
{
    constexpr size_t size = 5;
    std::array<bptr<A>, size> array;
    std::array<int, size> model;

    auto check = [&] {
        auto alive = 0;
        for (auto j = 0u; j < size; j++)
        {
            if (model[j] < 0)
            {
                check_empty(array[j]);
                continue;
            }
            auto count = 0u;
            bool first = true;
            for (auto m = 0u; m < size; m++)
            {
                count += model[m] == model[j];
                if (m < j && model[m] == model[j])
                    first = false;
            }
            alive += first;
            fm_assert(array[j]->val == model[j]);
            fm_assert(array[j].use_count() == count);
        }
        fm_assert(A_alive == alive);
    };

    for (auto n = 0u; n < size; n++)
    {
        A_total = 0; A_alive = 0;
        for (auto j = 0u; j <= n; j++)
            array[j] = bptr<A>{InPlace, (int)j};
        for (auto j = n+1; j < size; j++)
            array[j] = array[j % (n+1)];
        for (auto j = 0u; j < size; j++)
            model[j] = int(j % (n+1));
        fm_assert(A_total == int(n+1));
        check();

        for (auto k = 0u; k < size; k++)
            for (auto i = 1u; i < size; i++)
            {
                auto dst = i-1;
                auto src = (i+k) % size;
                if ((i+k) % 3 == 0)
                {
                    array[dst] = move(array[src]);
                    auto val = model[src];
                    model[src] = -1;
                    model[dst] = val;
                }
                else
                {
                    array[dst] = array[src];
                    model[dst] = model[src];
                }
                check();
            }

        for (auto j = 0u; j < size; j++)
        {
            array[j] = nullptr;
            model[j] = -1;
            check();
        }
        fm_assert(A_total == int(n+1));
        fm_assert(A_alive == 0);
        fm_assert(array == std::array<bptr<A>, size>{});
    }
}

template<typename P>
auto suite<P>::make_self_owned() -> Node*
{
    auto p = bptr<Node>{InPlace, 0, nullptr};
    p->next = p;
    return p.get();
}

// regression: releasing the last reference through a bptr stored inside the
// pointee must not touch that bptr after the pointee is deleted
template<typename P>
void suite<P>::test18()
{
    Node_alive = 0;

    make_self_owned()->next.reset();
    fm_assert(Node_alive == 0);

    make_self_owned()->next = nullptr;
    fm_assert(Node_alive == 0);

    {
        auto q = bptr<Node>{InPlace, 0, nullptr};
        make_self_owned()->next = q;
        fm_assert(Node_alive == 1);
        fm_assert(q.use_count() == 1);
    }
    fm_assert(Node_alive == 0);

    {
        auto q = bptr<Node>{InPlace, 0, nullptr};
        make_self_owned()->next = move(q);
        fm_assert(!q);
    }
    fm_assert(Node_alive == 0);

    make_self_owned()->next.reset(new Node{0, nullptr});
    fm_assert(Node_alive == 0);
}

// regression: destroy() through a bptr stored inside the pointee must not
// delete the pointee twice
template<typename P>
void suite<P>::test19()
{
    Node_alive = 0;

    make_self_owned()->next.destroy();
    fm_assert(Node_alive == 0);

    auto p = bptr<Node>{InPlace, 0, nullptr};
    p->next = p;
    auto q = p;
    p.destroy();
    fm_assert(Node_alive == 0);
    fm_assert(!p && !q);
    fm_assert(p.use_count() == 0 && q.use_count() == 0);
}

template<typename P>
struct suite<P>::Probe : bptr_base
{
    weak_bptr<Probe> weak;
    bool* locked;
    explicit Probe(bool* locked) : locked{locked} {}
    ~Probe() noexcept override { *locked = (bool)weak.lock(); }
    fm_DISABLE_MOVE_COPY(Probe);
};

// regression: weak_bptr::lock() from the pointee's destructor must fail
template<typename P>
void suite<P>::test20()
{
    bool locked = true;
    auto p = bptr<Probe>{InPlace, &locked};
    p->weak = p;
    auto q = p;
    p.destroy();
    fm_assert(!locked);

    locked = true;
    p = bptr<Probe>{InPlace, &locked};
    p->weak = p;
    p = nullptr;
    fm_assert(!locked);
}

template<typename P>
void suite<P>::test21()
{
    auto p = bptr<Foo>{InPlace, 19};
    p.destroy();
    fm_assert(p == nullptr);
    fm_assert(p == bptr<Foo>{});
    fm_assert(bptr<Foo>{} == p);
    fm_assert(p == bptr<const Foo>{});

    auto q = bptr<Foo>{InPlace, 19};
    fm_assert(p != q);
    q.destroy();
    fm_assert(p == q);
}

struct il_ctor : bptr_base
{
    int which;
    il_ctor(std::initializer_list<int>) : which{1} {}
    il_ctor(int, int) noexcept : which{2} {}
};

// regression: bptr{InPlace} brace-initializes, so its noexcept must follow the
// brace-init, which here picks the throwing initializer_list ctor
template<typename P>
void suite<P>::test22()
{
    static_assert(std::is_nothrow_constructible_v<il_ctor, int, int>);
    static_assert(!noexcept(bptr<il_ctor>{InPlace, 1, 2}));
    static_assert(noexcept(bptr<Baz>{InPlace}));

    fm_assert(bptr<il_ctor>{InPlace, 1, 2}->which == 1);
    fm_assert(bptr<il_ctor>{new il_ctor(1, 2)}->which == 2);
}

template<typename P>
void suite<P>::test23()
{
    auto p = bptr<Foo>{InPlace, 23};
    auto w = weak_bptr<Foo>{p};
    weak_bptr<const Foo> w2 = w;
    fm_assert(w2.lock().get() == p.get());
    fm_assert(w == weak_bptr<Foo>{p});
    fm_assert(!(w == weak_bptr<Foo>{}));

    weak_bptr<bptr_base> w3;
    w3 = w;
    fm_assert(w3.lock().get() == p.get());
    w3 = move(w);
    fm_assert(!w.lock());
    fm_assert(w3.lock().get() == p.get());
    fm_assert(w2.use_count() == 1);

    auto q = bptr<Foo>{InPlace, 24};
    auto w4 = weak_bptr<Foo>{q}, w5 = w4;
    q.destroy();
    p.destroy();
    fm_assert(w4 == w5);
    fm_assert(w4 == w2);
    fm_assert(w4 == weak_bptr<Foo>{});
    fm_assert(weak_bptr<Foo>{w4} == w4);
}

// destroy() with a weak_bptr alive frees the object and keeps the block
template<typename P>
void suite<P>::test25()
{
    for (bool from_raw : {false, true})
    {
        A_total = 0; A_alive = 0;
        auto p = from_raw ? bptr<A>{new A{25}} : bptr<A>{InPlace, 25};
        auto w = weak_bptr<A>{p};
        const A* raw = p.get();
        auto before = heap_snapshot<P>();
        p.destroy();
        auto after = heap_snapshot<P>();
        fm_assert(A_alive == 0 && !p && p.has_block());
        fm_assert(w.expired() && !w.lock());
        fm_assert(after.objects == before.objects - has_stats<P>);
        fm_assert(after.blocks == before.blocks);
        fm_assert(after.allocations == before.allocations);
        fm_assert(poisoned(raw));

        p = nullptr;
        fm_assert(heap_snapshot<P>().blocks == before.blocks);
        w = nullptr;
        fm_assert(heap_snapshot<P>().blocks == before.blocks - has_stats<P>);
    }
}

struct alignas(64) Aligned : bptr_base { char c = 0; };
struct alignas(64) Aligned_big : bptr_base { char data[2048]{}; };

template<typename P>
void suite<P>::test26()
{
    auto aligned = [](const void* p) { return (uintptr_t)p % 64 == 0; };
    fm_assert(aligned(bptr<Aligned>{InPlace}.get()));
    fm_assert(aligned(bptr<Aligned_big>{InPlace}.get()));
    fm_assert(aligned(bptr<Aligned>{new Aligned}.get()));
}

struct Big : bptr_base { char data[2048]{}; };

// an in-place object shares the block's allocation
template<typename P>
void suite<P>::test27()
{
    if constexpr (counts_allocations<P>)
    {
        auto allocations = [](auto make) {
            auto before = atomic_load(&P::allocator::total);
            auto p = make();
            fm_assert(p);
            return atomic_load(&P::allocator::total) - before;
        };
        A_total = 0; A_alive = 0;
        fm_assert(allocations([] { return bptr<A>{InPlace, 27}; }) == 1);
        fm_assert(allocations([] { return bptr<Big>{InPlace}; }) == 1);
        fm_assert(allocations([] { return bptr<A>{new A{27}}; }) == 1);
    }
}

namespace chaos {

constexpr uint32_t H = 6, W = 3, max_objs = 64, max_ops = 64, nseqs = 2000, none = (uint32_t)-1;

template<typename P> struct State;

struct no_weak {};

template<typename P>
struct Obj final : bptr_base
{
    using weak_type = std::conditional_t<P::has_weak, basic_weak_bptr<Obj, P>, no_weak>;

    State<P>& st;
    uint32_t id;
    basic_bptr<Obj, P> child;
    weak_type self;

    Obj(State<P>& st, uint32_t id) noexcept;
    ~Obj() noexcept override;
    fm_DISABLE_MOVE_COPY(Obj);
};

#define chaos_assert(...) ((__VA_ARGS__) ? void() : fail(#__VA_ARGS__, __LINE__))

template<typename P>
struct State
{
    using obj = Obj<P>;
    using hard_ptr = basic_bptr<obj, P>;
    using weak_ptr = typename obj::weak_type;
    static constexpr bool has_weak = P::has_weak;

    std::array<hard_ptr, H> hard;
    std::array<weak_ptr, W> weak;
    std::array<obj*, max_objs> raw{};
    std::array<bool, max_objs> alive{};
    uint32_t alive_count = 0;
    std::array<uint32_t, max_objs> died{};
    uint32_t ndied = 0;

    // Slots each object's destructor resets, destroys and clears. Reset and destroy slots are
    // disjoint, so a cascade ends the same whatever order the destructors run in.
    std::array<uint32_t, max_objs> kill_reset{}, kill_destroy{}, kill_weak{};

    std::array<uint32_t, H> m_hard;
    std::array<uint32_t, W> m_weak;
    std::array<uint32_t, max_objs> m_child{}, m_self{}, m_count{};
    std::array<bool, max_objs> m_alive{}, m_inplace{};
    uint32_t nobjs = 0;

    Random::xoshiro256starstar rng;
    const char* policy;
    uint32_t seq, op = 0, kind = 0, sub = 0;

    State(uint32_t seq, const char* policy) : policy{policy}, seq{seq}
    {
        m_hard.fill(none);
        m_weak.fill(none);
        Random::seed(rng, seq);
    }

    [[noreturn]] void fail(const char* expr, int line) const
    {
        fm_abort("bptr %s chaos: '%s' at line %d, seq %u op %u kind %u.%u", policy, expr, line, seq, op, kind, sub);
    }

    uint32_t rnd(uint32_t n) { return (uint32_t)(Random::next(rng) % n); }
    bool live(uint32_t k) const { return k != none && m_alive[k]; }
    obj* ptr(uint32_t k) const { return live(k) ? raw[k] : nullptr; }
    uint32_t count(uint32_t k) const { return live(k) ? m_count[k] : 0u; }

    uint32_t pick_live()
    {
        uint32_t n = 0;
        for (auto k = 0u; k < nobjs; k++)
            n += m_alive[k];
        if (!n)
            return none;
        auto r = rnd(n);
        for (auto k = 0u; ; k++)
            if (m_alive[k] && !r--)
                return k;
    }

    // A self-move leaves a bptr unchanged.
    static void hard_move(uint32_t& dst, uint32_t& src)
    {
        if (&dst != &src)
        {
            dst = src;
            src = none;
        }
    }

    // A self-move empties a weak_bptr.
    static void weak_move(uint32_t& dst, uint32_t& src)
    {
        auto k = &dst == &src ? none : src;
        src = none;
        dst = k;
    }

    void weak_copy(uint32_t& dst, uint32_t src) const
    {
        if (dst != src)
            dst = live(src) ? src : none;
    }

    void recount()
    {
        m_count = {};
        for (auto k : m_hard)
            if (k != none)
                m_count[k]++;
        for (auto k = 0u; k < nobjs; k++)
            if (m_alive[k] && m_child[k] != none)
                m_count[m_child[k]]++;
    }

    void die(uint32_t k)
    {
        m_alive[k] = false;
        m_child[k] = m_self[k] = none;
        if (kill_reset[k] != none)
            m_hard[kill_reset[k]] = none;
        if (kill_destroy[k] != none && live(m_hard[kill_destroy[k]]))
            die(m_hard[kill_destroy[k]]);
        if (kill_weak[k] != none)
            m_weak[kill_weak[k]] = none;
    }

    void settle()
    {
        for (bool again = true; again; )
        {
            again = false;
            recount();
            for (auto k = 0u; k < nobjs; k++)
                if (m_alive[k] && !m_count[k])
                {
                    die(k);
                    again = true;
                }
        }
    }

    void created(uint32_t id)
    {
        chaos_assert(id < nobjs && !alive[id]);
        alive[id] = true;
        alive_count++;
    }

    // Runs inside ~Obj, before its members release their blocks.
    void destroyed(obj& o)
    {
        auto id = o.id;
        chaos_assert(alive[id]);
        alive[id] = false;
        alive_count--;
        died[ndied++] = id;
        chaos_assert(o.child.get() != &o);
        if constexpr (has_weak)
            chaos_assert(o.self.lock().get() != &o);
        for (const auto& p : hard)
            chaos_assert(p.get() != &o);
        if constexpr (has_weak)
            for (const auto& w : weak)
                chaos_assert(w.lock().get() != &o);
        if (kill_reset[id] != none)
            hard[kill_reset[id]] = nullptr;
        if (kill_destroy[id] != none)
            hard[kill_destroy[id]].destroy();
        if constexpr (has_weak)
        {
            if (kill_weak[id] != none)
                weak[kill_weak[id]] = nullptr;
        }
    }

    void create(uint32_t i)
    {
        auto k = nobjs++;
        kill_reset[k] = rnd(3) ? none : rnd(H/2);
        kill_destroy[k] = rnd(3) ? none : H/2 + rnd(H - H/2);
        kill_weak[k] = !has_weak || rnd(3) ? none : rnd(W);
        m_alive[k] = true;
        m_child[k] = m_self[k] = none;
        sub = rnd(3);
        m_inplace[k] = sub == 0;
        switch (sub)
        {
        case 0: {
            auto p = hard_ptr{InPlace, *this, k};
            raw[k] = p.get();
            if constexpr (has_weak)
            {
                if (rnd(2))
                {
                    p->self = p;
                    m_self[k] = k;
                }
            }
            hard[i] = move(p);
            break;
        }
        case 1:
            raw[k] = new obj{*this, k};
            hard[i].reset(raw[k]);
            break;
        default:
            raw[k] = new obj{*this, k};
            hard[i] = hard_ptr{raw[k]};
            break;
        }
        m_hard[i] = k;
    }

    void slot_child(uint32_t i, uint32_t i2)
    {
        auto k = m_hard[i];
        auto& o = *hard[i];
        sub = rnd(5);
        switch (sub)
        {
        case 0:
            o.child = hard[i2];
            m_child[k] = m_hard[i2];
            break;
        case 1:
            o.child = move(hard[i2]);
            hard_move(m_child[k], m_hard[i2]);
            break;
        case 2:
            o.child = nullptr;
            m_child[k] = none;
            break;
        case 3:
            hard[i] = hard[i]->child;
            m_hard[i] = m_child[k];
            break;
        default:
            hard[i] = move(hard[i]->child);
            hard_move(m_hard[i], m_child[k]);
            break;
        }
    }

    void obj_child(uint32_t k, uint32_t k2, uint32_t i)
    {
        auto& o = *raw[k];
        auto& o2 = *raw[k2];
        sub = rnd(8);
        switch (sub)
        {
        case 0:
            o.child = hard[i];
            m_child[k] = m_hard[i];
            break;
        case 1:
            o.child = move(hard[i]);
            hard_move(m_child[k], m_hard[i]);
            break;
        case 2:
            o.child = o2.child;
            m_child[k] = m_child[k2];
            break;
        case 3:
            o.child = move(o2.child);
            hard_move(m_child[k], m_child[k2]);
            break;
        case 4:
            o.child.reset();
            m_child[k] = none;
            break;
        case 5: {
            auto c = m_child[k];
            o.child.destroy();
            if (live(c))
                die(c);
            break;
        }
        case 6:
            hard[i] = o.child;
            m_hard[i] = m_child[k];
            break;
        default:
            hard[i] = move(o.child);
            hard_move(m_hard[i], m_child[k]);
            break;
        }
    }

    void obj_self(uint32_t k, uint32_t k2, uint32_t i, uint32_t j)
    {
        auto& o = *raw[k];
        auto& o2 = *raw[k2];
        sub = rnd(9);
        switch (sub)
        {
        case 0:
            o.self = hard[i];
            weak_copy(m_self[k], m_hard[i]);
            break;
        case 1:
            o.self = weak[j];
            weak_copy(m_self[k], m_weak[j]);
            break;
        case 2:
            o.self = move(weak[j]);
            weak_move(m_self[k], m_weak[j]);
            break;
        case 3:
            o.self = nullptr;
            m_self[k] = none;
            break;
        case 4:
            weak[j] = o.self;
            weak_copy(m_weak[j], m_self[k]);
            break;
        case 5:
            weak[j] = move(o.self);
            weak_move(m_weak[j], m_self[k]);
            break;
        case 6:
            o.self = o2.self;
            weak_copy(m_self[k], m_self[k2]);
            break;
        case 7:
            o.self = move(o2.self);
            weak_move(m_self[k], m_self[k2]);
            break;
        default:
            hard[i] = o.self.lock();
            m_hard[i] = live(m_self[k]) ? m_self[k] : none;
            break;
        }
    }

    void weak_step(uint32_t i, uint32_t j, uint32_t j2, uint32_t k, uint32_t k2)
    {
        switch (kind)
        {
        case 9:
            weak[j] = hard[i];
            weak_copy(m_weak[j], m_hard[i]);
            break;
        case 10:
            weak[j] = weak[j2];
            weak_copy(m_weak[j], m_weak[j2]);
            break;
        case 11:
            weak[j] = move(weak[j2]);
            weak_move(m_weak[j], m_weak[j2]);
            break;
        case 12:
            sub = rnd(2);
            if (sub)
                weak[j].reset();
            else
                weak[j] = nullptr;
            m_weak[j] = none;
            break;
        case 13:
            weak[j].swap(weak[j2]);
            swap(m_weak[j], m_weak[j2]);
            break;
        case 14:
            hard[i] = weak[j].lock();
            m_hard[i] = live(m_weak[j]) ? m_weak[j] : none;
            break;
        default:
            if (k != none)
                obj_self(k, k2, i, j);
            break;
        }
    }

    void step()
    {
        auto i = rnd(H), i2 = rnd(H), j = rnd(W), j2 = rnd(W);
        auto k = pick_live(), k2 = pick_live();
        // Without weak_bptr, kinds 9-14 and 19-20 don't exist.
        if constexpr (has_weak)
            kind = rnd(21);
        else
        {
            kind = rnd(13);
            if (kind >= 9)
                kind += 6;
        }
        sub = 0;
        switch (kind)
        {
        case 0: case 1: case 2:
            if (nobjs < max_objs)
                create(i);
            break;
        case 3:
            hard[i] = hard[i2];
            m_hard[i] = m_hard[i2];
            break;
        case 4:
            hard[i] = move(hard[i2]);
            hard_move(m_hard[i], m_hard[i2]);
            break;
        case 5:
            sub = rnd(2);
            if (sub)
                hard[i].reset();
            else
                hard[i] = nullptr;
            m_hard[i] = none;
            break;
        case 6:
            hard[i].destroy();
            if (live(m_hard[i]))
                die(m_hard[i]);
            break;
        case 7:
            hard[i].swap(hard[i2]);
            swap(m_hard[i], m_hard[i2]);
            break;
        case 8:
            sub = rnd(2);
            if (sub)
                hard[i] = static_pointer_cast<obj>(basic_bptr<bptr_base, P>{hard[i2]});
            else
                hard[i] = static_pointer_cast<obj>(hard[i2]);
            m_hard[i] = live(m_hard[i2]) ? m_hard[i2] : none;
            break;
        case 15: case 16:
            if (live(m_hard[i]))
                slot_child(i, i2);
            break;
        case 17: case 18:
            if (k != none)
                obj_child(k, k2, i);
            break;
        default:
            if constexpr (has_weak)
                weak_step(i, j, j2, k, k2);
            break;
        }
        settle();
        check();
    }

    void check_hard(const hard_ptr& p, uint32_t k) const
    {
        chaos_assert(p.get() == ptr(k));
        chaos_assert((bool)p == live(k));
        chaos_assert((p == nullptr) == !live(k));
        chaos_assert(p.has_block() == (k != none));
        chaos_assert(p.use_count() == count(k));
    }

    void check_weak(const weak_ptr& w, uint32_t k) const
    {
        chaos_assert(w.use_count() == count(k));
        chaos_assert(w.expired() == !live(k));
        auto p = w.lock();
        chaos_assert(p.get() == ptr(k));
        chaos_assert(p.use_count() == (live(k) ? count(k) + 1 : 0u));
    }

    // A block lives while any hard or weak reference names it, even after destroy().
    int64_t model_blocks() const
    {
        std::array<bool, max_objs> used{};
        for (auto k : m_hard)
            if (k != none)
                used[k] = true;
        for (auto k : m_weak)
            if (k != none)
                used[k] = true;
        for (auto k = 0u; k < nobjs; k++)
        {
            if (m_child[k] != none)
                used[m_child[k]] = true;
            if (m_self[k] != none)
                used[m_self[k]] = true;
        }
        int64_t n = 0;
        for (auto k = 0u; k < nobjs; k++)
            n += used[k];
        return n;
    }

    void check_memory()
    {
        // A pool keeps a freed block's storage addressable.
        for (auto d = 0u; d < ndied; d++)
            if (!pooling<P> || !m_inplace[died[d]])
                chaos_assert(poisoned(raw[died[d]]));
        ndied = 0;
        if constexpr (has_stats<P>)
        {
            chaos_assert(atomic_load(&P::stats::live_objects) == (int64_t)alive_count);
            chaos_assert(atomic_load(&P::stats::live_blocks) == model_blocks());
        }
        if constexpr (counts_allocations<P>)
            chaos_assert(atomic_load(&P::allocator::live) == model_blocks());
    }

    void check()
    {
        recount();
        uint32_t n = 0;
        for (auto k = 0u; k < nobjs; k++)
        {
            chaos_assert(alive[k] == m_alive[k]);
            if (!m_alive[k])
                continue;
            n++;
            check_hard(raw[k]->child, m_child[k]);
            if constexpr (has_weak)
                check_weak(raw[k]->self, m_self[k]);
        }
        chaos_assert(alive_count == n);
        for (auto i = 0u; i < H; i++)
        {
            check_hard(hard[i], m_hard[i]);
            for (auto i2 = 0u; i2 < H; i2++)
            {
                auto eq = ptr(m_hard[i]) == ptr(m_hard[i2]);
                chaos_assert((hard[i] == hard[i2]) == eq);
                chaos_assert((hard[i] == basic_bptr<const obj, P>{hard[i2]}) == eq);
            }
        }
        if constexpr (has_weak)
            for (auto j = 0u; j < W; j++)
            {
                check_weak(weak[j], m_weak[j]);
                for (auto j2 = 0u; j2 < W; j2++)
                    chaos_assert((weak[j] == weak[j2]) == (ptr(m_weak[j]) == ptr(m_weak[j2])));
            }
        check_memory();
    }

    // Breaks every cycle first, so emptying the slots must free every object and block.
    void finish()
    {
        for (auto k = 0u; k < nobjs; k++)
            if (m_alive[k])
            {
                raw[k]->child = nullptr;
                m_child[k] = none;
                settle();
                check();
            }
        for (auto i = 0u; i < H; i++)
        {
            hard[i] = nullptr;
            m_hard[i] = none;
            settle();
            check();
        }
        if constexpr (has_weak)
        {
            for (auto& w : weak)
                w = nullptr;
            m_weak.fill(none);
        }
        chaos_assert(alive_count == 0);
        check_memory();
    }
};

#undef chaos_assert

template<typename P> Obj<P>::Obj(State<P>& st, uint32_t id) noexcept: st{st}, id{id} { st.created(id); }
template<typename P> Obj<P>::~Obj() noexcept { st.destroyed(*this); }

template<typename P>
void run(const char* policy)
{
    for (auto seq = 0u; seq < nseqs; seq++)
    {
        auto before = heap_snapshot<P>();
        {
            State<P> st{seq, policy};
            auto nops = 1 + st.rnd(max_ops);
            for (; st.op < nops; st.op++)
                st.step();
            st.finish();
        }
        check_leaks<P>(before, policy, "chaos seq", seq);
    }
}

} // namespace chaos

void test28()
{
    using P = counted_refcount;
    auto total = [] { return atomic_load(&P::allocator::total); };
    auto t0 = total();
    Custom_disposed = 0;
    {
        auto p = basic_bptr<Custom, P>{new Custom{28}};
        fm_assert(p->x == 28 && total() == t0);
        auto q = p;
        p.destroy();
        fm_assert(Custom_disposed == 1 && !q);
    }
    {
        auto p = basic_bptr<Custom, P>{InPlace, 28};
        fm_assert(p->x == 28 && total() == t0 + 1);
        auto q = basic_bptr<Foo, P>{InPlace, 28};
        fm_assert(q->x == 28 && total() == t0 + 2);
    }
    fm_assert(Custom_disposed == 1);
}

// a bptr made on another thread can move to this one before its first copy
void test29()
{
    using P = thread_counted_refcount;
    auto before = heap_snapshot<P>(false);
    A_total = 0; A_alive = 0;
    {
        basic_bptr<A, P> p;
        std::thread{[&p] { p = basic_bptr<A, P>{InPlace, 29}; }}.join();
        auto q = p;
        auto w = basic_weak_bptr<A, P>{p};
        fm_assert(q.use_count() == 2);
        fm_assert(w.lock()->val == 29);
        p = nullptr;
        q = nullptr;
        fm_assert(A_alive == 0 && w.expired());
    }
    check_leaks<P>(before, "thread_counted", "test", 29, false);
}

volatile int32_t Ct_alive; // NOLINT

struct Ct : bptr_base
{
    int val = 30;
    Ct() noexcept { atomic_fetch_add(&Ct_alive, 1, memory_order::relaxed); }
    ~Ct() noexcept override
    {
        // Under TSan, a missing happens-before edge shows up as this write racing with readers.
        val = -1;
        fm_assert(atomic_fetch_sub(&Ct_alive, 1, memory_order::relaxed) > 0);
    }
    fm_DISABLE_MOVE_COPY(Ct);
};

// threads copy and lock one block while its last reference drops, or while destroy() runs
void test30()
{
    using P = atomic_counted_refcount;
    using ptr = basic_bptr<Ct, P>;
    constexpr uint32_t nthreads = 4, rounds = 300, iters = 256;
    for (auto round = 0u; round < rounds; round++)
    {
        auto before = heap_snapshot<P>(false);
        bool destroy = round % 3 == 1, weak_only = round % 3 == 2;
        {
            auto p = ptr{InPlace};
            auto w = basic_weak_bptr<Ct, P>{p};
            std::latch start{nthreads + 1};
            std::array<std::thread, nthreads> threads;
            for (auto t = 0u; t < nthreads; t++)
                threads[t] = std::thread{[&start, destroy, drop = (t * 97 + round * 31) % iters, mine = weak_only ? ptr{} : p, weak = w]() mutable {
                    start.arrive_and_wait();
                    for (auto i = 0u; i < iters; i++)
                    {
                        if (i == drop)
                            mine = nullptr;
                        auto copy = mine;
                        auto weak2 = weak;
                        auto locked = weak2.lock();
                        // destroy() frees the object under live references.
                        if (!destroy && locked)
                            fm_assert(locked->val == 30);
                    }
                }};
            start.arrive_and_wait();
            if (destroy)
                p.destroy();
            p = nullptr;
            for (auto& t : threads)
                t.join();
            fm_assert(atomic_load(&Ct_alive) == 0);
            fm_assert(w.expired() && !w.lock());
        }
        check_leaks<P>(before, "atomic_counted", "test30 round", round, false);
    }
}

// bptr and weak_bptr comparisons across base, derived and const types hold after destroy().
template<typename P>
void suite<P>::test31()
{
    auto copies = [] {
        if constexpr (has_stats<P>)
            return (int64_t)atomic_load(&P::stats::copies);
        else
            return (int64_t)0;
    };

    auto d = bptr<Bar>{InPlace, 31}, e = bptr<Bar>{InPlace, 37};
    bptr<Foo> b = d;
    bptr<const Foo> cb = d;
    bptr<const Bar> cd = d;
    auto check = [&] {
        auto before = copies();
        fm_assert(b == d && d == b && cb == d && d == cb && b == cd && cd == b);
        fm_assert(b != e && e != cb);
        fm_assert(copies() == before);
    };
    check();
    d.destroy();
    fm_assert(!b && e);
    check();

    if constexpr (has_weak)
    {
        auto f = bptr<Bar>{InPlace, 41};
        auto wd = weak_bptr<Bar>{f};
        weak_bptr<Foo> wb = f, we = e;
        weak_bptr<const Foo> wcb = f;
        weak_bptr<const Bar> wcd = f;
        auto check_weak = [&] {
            fm_assert(wb == wd && wd == wb && wcb == wd && wd == wcb && wb == wcd && wcd == wb);
            fm_assert(wd != we && we != wcd);
        };
        check_weak();
        f.destroy();
        fm_assert(wd.expired() && !we.expired());
        check_weak();
    }
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
    run_test<P>(policy, 8, test8);
    run_test<P>(policy, 9, test9);
    run_test<P>(policy, 10, test10);
    run_test<P>(policy, 11, test11);
    run_test<P>(policy, 12, test12);
    if constexpr (has_weak)
    {
        run_test<P>(policy, 13, test13);
        run_test<P>(policy, 14, test14);
    }
    run_test<P>(policy, 15, test15);
    run_test<P>(policy, 16, test16);
    run_test<P>(policy, 17, test17);
    run_test<P>(policy, 18, test18);
    run_test<P>(policy, 19, test19);
    if constexpr (has_weak)
        run_test<P>(policy, 20, test20);
    run_test<P>(policy, 21, test21);
    run_test<P>(policy, 22, test22);
    if constexpr (has_weak)
        run_test<P>(policy, 23, test23);
    chaos::run<P>(policy);
    if constexpr (has_weak)
        run_test<P>(policy, 25, test25);
    run_test<P>(policy, 26, test26);
    run_test<P>(policy, 27, test27);
    run_test<P>(policy, 31, test31);
}

} // namespace

void Test::test_bptr()
{
    suite<non_atomic_refcount>::run("non_atomic");
    suite<atomic_refcount>::run("atomic");
    suite<thread_checked_refcount>::run("thread_checked");
    suite<counted_refcount>::run("counted");
    suite<small_refcount>::run("small");
    suite<pooled_refcount>::run("pooled");
    suite<strong_refcount>::run("strong");
    run_test<counted_refcount>("counted", 28, test28);
    test29();
    test30();
}

} // namespace floormat
