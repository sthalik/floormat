#include "app.hpp"
#include "compat/inplace-function.hpp"
#include "compat/function2.hpp"
#include <cr/Pointer.h>
#include <functional>
#include <memory>

// Outside the anonymous namespace, where clang warns about their unused members.
namespace floormat::inplace_function_test {

struct move_only_call
{
    move_only_call();
    move_only_call(move_only_call&&) noexcept;
    u64 operator()(u64 x) const noexcept;
};

struct late_arg;

struct holds_late
{
    inplace_function<u64(late_arg), 8> f;
};

struct late_arg
{
    u64 v;
};

struct alignas(16) over_aligned
{
    u64 v;
    u64 operator()(u64 x) const noexcept;
};

struct throwing_move
{
    u64 v;
    throwing_move();
    throwing_move(throwing_move&& o) noexcept(false);
    u64 operator()(u64 x) const noexcept;
};

struct throwing_dtor
{
    u64 v;
    ~throwing_dtor() noexcept(false);
    u64 operator()(u64 x) const noexcept;
};

struct wrong_signature
{
    void operator()(const char*) const noexcept;
};

struct non_const_call
{
    u64 operator()(u64 x) noexcept;
};

struct const_call_deleted
{
    u64 operator()(u64 x) noexcept;
    u64 operator()(u64 x) const noexcept = delete;
};

struct may_throw
{
    u64 operator()(u64 x) const;
};

struct lvalue_only
{
    u64 operator()(u64 x) & noexcept;
};

struct returns_value
{
    u64 operator()() const noexcept;
};

} // namespace floormat::inplace_function_test

namespace floormat {

namespace detail {
static_assert(!inplace_function<void(), 8, 8>{});
} // namespace detail

namespace {

using namespace inplace_function_test;
using F = inplace_function<u64(u64), 40, 8>;
using C = inplace_function<u64(u64) const, 40, 8>;

template<u32 N>
struct sized
{
    u64 v[N / 8] = {};
    u64 operator()(u64 x) const noexcept { return x + v[0]; }
};

struct address_probe
{
    u64 pad[3] = {};
    const void* operator()() const noexcept { return this; }
};

struct alignas(32) aligned_probe
{
    u64 pad[4] = {};
    const void* operator()() const noexcept { return this; }
};

struct counted
{
    i32* alive;
    u64 v;

    constexpr counted(i32* alive, u64 v) noexcept: alive{alive}, v{v} { ++*alive; }
    constexpr counted(const counted& o) noexcept: alive{o.alive}, v{o.v} { ++*alive; }
    constexpr counted(counted&& o) noexcept: alive{o.alive}, v{o.v} { ++*alive; }
    constexpr ~counted() noexcept { --*alive; }
    constexpr u64 operator()(u64 x) const noexcept { return x * v; }
};

struct mutable_sum
{
    mutable u64 sum = 0;
    constexpr u64 operator()(u64 x) const noexcept { return sum += x; }
};

struct both_calls
{
    constexpr u64 operator()(u64 x) noexcept { return x + 1; }
    constexpr u64 operator()(u64 x) const noexcept { return x + 2; }
};

struct by_type
{
    constexpr u64 operator()(u64 x) const noexcept { return x * 2; }
    constexpr u64 operator()(u32 x) const noexcept { return x * 3; }
};

struct nodiscard_call
{
    [[nodiscard]] constexpr u64 operator()(u64 x) const noexcept { return x; }
};

struct no_address
{
    constexpr u64 operator()(u64 x) const noexcept { return x + 9; }
    void operator&() const = delete;
};

struct qualified_probe
{
    constexpr u64 operator()(u64 x) & noexcept { return x + 1; }
    constexpr u64 operator()(u64 x) const& noexcept { return x + 2; }
    constexpr u64 operator()(u64 x) volatile& noexcept { return x + 3; }
    constexpr u64 operator()(u64 x) const volatile& noexcept { return x + 4; }
    constexpr u64 operator()(u64 x) && noexcept { return x + 5; }
    constexpr u64 operator()(u64 x) const&& noexcept { return x + 6; }
    constexpr u64 operator()(u64 x) volatile&& noexcept { return x + 7; }
    constexpr u64 operator()(u64 x) const volatile&& noexcept { return x + 8; }
};

template<typename Signature> using Q = inplace_function<Signature, 8, 8>;

template<typename T> using lv = T&;
template<typename T> using clv = const T&;
template<typename T> using vlv = volatile T&;
template<typename T> using cvlv = const volatile T&;
template<typename T> using rv = T&&;
template<typename T> using crv = const T&&;
template<typename T> using vrv = volatile T&&;
template<typename T> using cvrv = const volatile T&&;

template<typename Signature, template<typename> typename Object>
constexpr u64 call_probe(u64 x)
{
    Q<Signature> f = qualified_probe{};
    return static_cast<Object<Q<Signature>>>(f)(x);
}

struct wide_arg
{
    u64 v[3];
};

struct pair_arg
{
    u32 a, b;
};

struct moved_arg
{
    u64 v;
    constexpr explicit moved_arg(u64 v) noexcept: v{v} {}
    constexpr moved_arg(moved_arg&& o) noexcept: v{o.v} { o.v = 0; }
};

struct member_probe
{
    u64 v;
    constexpr u64 add(u64 x) const noexcept { return v + x; }
    constexpr u64 twice() && noexcept { return v * 2; }
};

struct derived_probe : member_probe {};

struct maybe_empty
{
    bool full;
    constexpr explicit operator bool() const noexcept { return full; }
    constexpr u64 operator()(u64 x) const noexcept { return x + 1; }
};

constexpr u64 triple(u64 x) noexcept { return 3 * x; }

using CF = inplace_copyable_function<u64(u64), 40, 8>;
using add_fn = u64 (member_probe::*)(u64) const noexcept;

struct successor
{
    inplace_function<u64(), 8, 8> next;
    inplace_function<u64(), 32, 8>* slot;
    constexpr u64 operator()() noexcept
    {
        *slot = std::move(next);
        return 1;
    }
};

using owned_slot = inplace_function<u64(), 16, 8>;

struct owner
{
    std::unique_ptr<owned_slot> next;
    owned_slot* slot;
    constexpr u64 operator()() noexcept
    {
        *slot = std::move(*next);
        return 2;
    }
};

struct copier
{
    inplace_copyable_function<u64(), 8, 8> next;
    inplace_copyable_function<u64(), 32, 8>* slot;
    constexpr u64 operator()() noexcept
    {
        *slot = next;
        return 3;
    }
};

struct value_fn
{
    u64 k;
    constexpr u64 operator()() const noexcept { return k; }
};

struct converter
{
    value_fn next;
    inplace_function<u64(), 16, 8>* slot;
    constexpr u64 operator()() noexcept
    {
        *slot = std::move(next);
        return 4;
    }
};

static_assert(std::is_nothrow_move_constructible_v<F>);
static_assert(std::is_nothrow_move_assignable_v<F>);
static_assert(!std::is_copy_constructible_v<F>);
static_assert(!std::is_copy_assignable_v<F>);

static_assert(std::is_nothrow_constructible_v<F, sized<40>>);
static_assert(std::is_assignable_v<F&, sized<40>>);
static_assert(!std::is_constructible_v<F, sized<48>>);
static_assert(!std::is_assignable_v<F&, sized<48>>);

static_assert(!std::is_constructible_v<F, over_aligned>);
static_assert(std::is_constructible_v<inplace_function<u64(u64), 40, 16>, over_aligned>);
static_assert(!std::is_constructible_v<F, throwing_move>);
static_assert(!std::is_constructible_v<F, throwing_dtor>);
static_assert(!std::is_constructible_v<F, wrong_signature>);

static_assert(!std::is_constructible_v<F, fu2::unique_function<u64(u64)>>);
static_assert(!std::is_constructible_v<F, fu2::function_view<u64(u64)>>);
static_assert(std::is_constructible_v<F, inplace_function<u64(u64), 16, 8>>);

static_assert(std::is_constructible_v<F, decltype(triple)&>);
static_assert(std::is_constructible_v<F, u64(*)(u64)>);

static_assert(std::is_invocable_r_v<u64, const C&, u64>);
static_assert(!std::is_invocable_v<const F&, u64>);
static_assert(std::is_constructible_v<F, non_const_call>);
static_assert(!std::is_constructible_v<C, non_const_call>);
static_assert(std::is_constructible_v<F, const_call_deleted>);
static_assert(!std::is_constructible_v<C, const_call_deleted>);
static_assert(std::is_constructible_v<F, inplace_function<u64(u64) const, 16, 8>>);
static_assert(!std::is_constructible_v<C, inplace_function<u64(u64), 16, 8>>);
static_assert(std::is_constructible_v<fu2::function_view<u64(u64) const>, const C&>);
static_assert(std::is_constructible_v<fu2::function_view<u64(u64)>, F&>);

static_assert(std::is_invocable_v<Q<u64(u64)>&&, u64>);
static_assert(std::is_invocable_v<Q<u64(u64) &>&, u64>);
static_assert(!std::is_invocable_v<Q<u64(u64) &>&&, u64>);
static_assert(!std::is_invocable_v<Q<u64(u64) &&>&, u64>);
static_assert(std::is_invocable_v<Q<u64(u64) const&>&&, u64>);
static_assert(!std::is_invocable_v<const Q<u64(u64) const&&>&, u64>);
static_assert(!std::is_invocable_v<const Q<u64(u64) volatile>&, u64>);
static_assert(!std::is_invocable_v<volatile Q<u64(u64) const>&, u64>);
static_assert(std::is_nothrow_invocable_v<Q<u64(u64) noexcept>&, u64>);
static_assert(std::is_nothrow_invocable_v<const volatile Q<u64(u64) const volatile&& noexcept>&&, u64>);
static_assert(!std::is_nothrow_invocable_v<Q<u64(u64)>&, u64>);

static_assert(std::is_constructible_v<Q<u64(u64)>, may_throw>);
static_assert(!std::is_constructible_v<Q<u64(u64) noexcept>, may_throw>);
static_assert(std::is_constructible_v<Q<u64(u64) &>, lvalue_only>);
static_assert(!std::is_constructible_v<Q<u64(u64) &&>, lvalue_only>);
static_assert(!std::is_constructible_v<Q<u64(u64) volatile>, sized<8>>);
static_assert(std::is_constructible_v<Q<u64(u64) volatile>, u64(*)(u64)>);
static_assert(std::is_constructible_v<inplace_function<u64(), 8, 8>, returns_value>);
static_assert(!std::is_constructible_v<inplace_function<const u64&(), 8, 8>, returns_value>);
static_assert(std::is_constructible_v<fu2::function_view<u64(u64) volatile>, volatile Q<u64(u64) volatile>&>);

static_assert(std::is_copy_constructible_v<CF>);
static_assert(std::is_copy_assignable_v<CF>);
static_assert(std::is_nothrow_move_constructible_v<CF>);
static_assert(std::is_constructible_v<F, move_only_call>);
static_assert(!std::is_constructible_v<CF, move_only_call>);
static_assert(!std::is_assignable_v<CF&, move_only_call>);
static_assert(std::is_constructible_v<F, const CF&>);
static_assert(std::is_nothrow_constructible_v<F, CF>);
static_assert(std::is_assignable_v<F&, const CF&>);
static_assert(!std::is_constructible_v<CF, F>);
static_assert(!std::is_assignable_v<CF&, F>);

// The source is bigger than the target's capacity, so only moving its target across works.
static_assert(std::is_nothrow_constructible_v<inplace_function<u64(u64), 16, 8>, inplace_function<u64(u64), 8, 8>>);
static_assert(std::is_constructible_v<inplace_function<u64(u64), 40, 16>, inplace_function<u64(u64), 16, 8>>);
static_assert(!std::is_constructible_v<inplace_function<u64(u64), 16, 8>, F>);
static_assert(!std::is_constructible_v<F, inplace_function<u64(u64), 16, 16>>);

static_assert(std::is_nothrow_constructible_v<F, std::nullptr_t>);
static_assert(std::is_nothrow_assignable_v<F&, std::nullptr_t>);
static_assert(alignof(inplace_function<void(), 40>) == alignof(void*));
static_assert(sizeof(holds_late) > 0);

static_assert(std::is_constructible_v<inplace_function<u64(const member_probe&, u64) noexcept, 16, 8>, add_fn>);
static_assert(!std::is_constructible_v<inplace_function<u64(member_probe&), 16, 8>, decltype(&member_probe::twice)>);
static_assert(std::is_constructible_v<inplace_function<u64(member_probe&&), 16, 8>, decltype(&member_probe::twice)>);

constexpr F empty_constant;
static_assert(!empty_constant);
constinit F empty_slot;
constexpr F moved_empty = [] { F a; return F{std::move(a)}; }();
static_assert(!moved_empty);
constexpr F reassigned = [] { F a = triple; a = F{}; return a; }();
static_assert(!reassigned);

constexpr bool test_semantics()
{
    F f = [a = u64{7919}, b = u64{104729}](u64 x) noexcept { return x * a + b; };
    fm_assert(f);
    fm_assert(f(13) == 13 * 7919 + 104729);

    F g = triple;
    fm_assert(g(17) == 51);
    F h{&triple};
    fm_assert(h(19) == 57);

    inplace_function<u64(u64), 16, 8> small = [k = u64{31}](u64 x) noexcept { return x ^ k; };
    F nested{std::move(small)};
    fm_assert(!small);
    fm_assert(nested(97) == (97 ^ 31));

    F empty;
    fm_assert(!empty);

    F m{std::move(f)};
    fm_assert(!f);
    fm_assert(m(13) == 13 * 7919 + 104729);
    F n;
    n = std::move(m);
    fm_assert(!m);
    fm_assert(n(29) == 29 * 7919 + 104729);
    F& alias = n;
    n = std::move(alias);
    fm_assert(n(29) == 29 * 7919 + 104729);

    F counter = [sum = u64{0}](u64 x) mutable noexcept { return sum += x; };
    counter(5);
    fm_assert(counter(7) == 12);
    F moved{std::move(counter)};
    fm_assert(moved(1) == 13);

    inplace_function<void(u64&), 8, 8> add = [](u64& x) noexcept { x += 3; return x; };
    u64 v = 4;
    add(v);
    fm_assert(v == 7);
    inplace_function<void(u64&), 8, 8> bump = [](u64& x) noexcept { x += 5; };
    bump(v);
    fm_assert(v == 12);
    inplace_function<void(), 8, 8> inc = [p = &v]() noexcept { ++*p; };
    inc();
    fm_assert(v == 13);

    const C c = [k = u64{7}](u64 x) noexcept { return x * k; };
    fm_assert(c(3) == 21);
    C sum = mutable_sum{};
    sum(11);
    fm_assert(sum(13) == 24);
    C sum2{std::move(sum)};
    fm_assert(sum2(17) == 41);
    inplace_function<u64(u64) const, 16, 8> small_const = [](u64 x) noexcept { return x + 1; };
    F from_const{std::move(small_const)};
    fm_assert(from_const(41) == 42);

    F both_f = both_calls{};
    fm_assert(both_f(10) == 11);
    const C both_c = both_calls{};
    fm_assert(both_c(10) == 12);
    F typed = by_type{};
    fm_assert(typed(10) == 20);
    inplace_function<u64(u32), 8, 8> typed32 = by_type{};
    fm_assert(typed32(10) == 30);

    inplace_function<void(u64), 8, 8> discard = nodiscard_call{};
    discard(1);

    F no_addr = no_address{};
    F no_addr2{std::move(no_addr)};
    fm_assert(no_addr2(1) == 10);

    using fnptr = u64(*)(u64);
    F null_ptr = fnptr{nullptr};
    fm_assert(!null_ptr);
    F reset = triple;
    reset = fnptr{nullptr};
    fm_assert(!reset);

    F target = triple;
    F empty_source;
    target = std::move(empty_source);
    fm_assert(!target);

    using mixed = u64(wide_arg, pair_arg, moved_arg, u64&);
    inplace_function<mixed, 8, 8> args = [](wide_arg w, pair_arg p, moved_arg m, u64& out) noexcept {
        out = m.v;
        return w.v[0] * w.v[1] * w.v[2] + p.a * p.b;
    };
    u64 out = 0;
    fm_assert(args(wide_arg{{3, 5, 7}}, pair_arg{11, 13}, moved_arg{1009}, out) == 3 * 5 * 7 + 11 * 13);
    fm_assert(out == 1009);

    holds_late late{[](late_arg a) noexcept { return a.v * 2; }};
    fm_assert(late.f(late_arg{21}) == 42);

    i32 alive = 0;
    {
        F c = counted{&alive, 5};
        fm_assert(alive == 1);
        fm_assert(c(11) == 55);
        c = counted{&alive, 7};
        fm_assert(alive == 1);
        fm_assert(c(13) == 91);
        F d{std::move(c)};
        fm_assert(alive == 1);
        fm_assert(d(2) == 14);
        c = std::move(d);
        fm_assert(alive == 1);
        c = [](u64 x) noexcept { return x; };
        fm_assert(alive == 0);
        c = counted{&alive, 3};
        fm_assert(alive == 1);
        F e = counted{&alive, 2};
        fm_assert(alive == 2);
        e = std::move(c);
        fm_assert(alive == 1);
        fm_assert(e(5) == 15);
    }
    fm_assert(alive == 0);

    return true;
}
static_assert(test_semantics());

constexpr bool test_qualifiers()
{
    fm_assert(call_probe<u64(u64), lv>(100) == 101);
    fm_assert(call_probe<u64(u64), rv>(100) == 101);
    fm_assert(call_probe<u64(u64) const, clv>(100) == 102);
    fm_assert(call_probe<u64(u64) const, rv>(100) == 102);
    fm_assert(call_probe<u64(u64) volatile, vlv>(100) == 103);
    fm_assert(call_probe<u64(u64) const volatile, cvlv>(100) == 104);
    fm_assert(call_probe<u64(u64) &, lv>(100) == 101);
    fm_assert(call_probe<u64(u64) const&, clv>(100) == 102);
    fm_assert(call_probe<u64(u64) const&, rv>(100) == 102);
    fm_assert(call_probe<u64(u64) volatile&, vlv>(100) == 103);
    fm_assert(call_probe<u64(u64) const volatile&, cvlv>(100) == 104);
    fm_assert(call_probe<u64(u64) &&, rv>(100) == 105);
    fm_assert(call_probe<u64(u64) const&&, crv>(100) == 106);
    fm_assert(call_probe<u64(u64) const&&, rv>(100) == 106);
    fm_assert(call_probe<u64(u64) volatile&&, vrv>(100) == 107);
    fm_assert(call_probe<u64(u64) const volatile&&, cvrv>(100) == 108);

    fm_assert(call_probe<u64(u64) noexcept, lv>(100) == 101);
    fm_assert(call_probe<u64(u64) const noexcept, clv>(100) == 102);
    fm_assert(call_probe<u64(u64) volatile noexcept, vlv>(100) == 103);
    fm_assert(call_probe<u64(u64) const volatile noexcept, cvlv>(100) == 104);
    fm_assert(call_probe<u64(u64) & noexcept, lv>(100) == 101);
    fm_assert(call_probe<u64(u64) const& noexcept, clv>(100) == 102);
    fm_assert(call_probe<u64(u64) volatile& noexcept, vlv>(100) == 103);
    fm_assert(call_probe<u64(u64) const volatile& noexcept, cvlv>(100) == 104);
    fm_assert(call_probe<u64(u64) && noexcept, rv>(100) == 105);
    fm_assert(call_probe<u64(u64) const&& noexcept, crv>(100) == 106);
    fm_assert(call_probe<u64(u64) volatile&& noexcept, vrv>(100) == 107);
    fm_assert(call_probe<u64(u64) const volatile&& noexcept, cvrv>(100) == 108);

    Q<u64(u64) volatile> v = qualified_probe{};
    volatile auto& vr = v;
    fm_assert(vr);
    fm_assert(vr(100) == 103);

    return true;
}
static_assert(test_qualifiers());

constexpr bool test_members()
{
    member_probe p{100};
    const derived_probe d{{200}};

    inplace_function<u64(const member_probe&, u64) noexcept, 16, 8> by_ref = &member_probe::add;
    fm_assert(by_ref(p, 1) == 101);
    inplace_function<u64(const derived_probe&, u64), 16, 8> by_derived = &member_probe::add;
    fm_assert(by_derived(d, 2) == 202);
    inplace_function<u64(const member_probe*, u64), 16, 8> by_ptr = &member_probe::add;
    fm_assert(by_ptr(&p, 3) == 103);
    inplace_function<u64(std::reference_wrapper<const member_probe>, u64), 16, 8> by_wrapper = &member_probe::add;
    fm_assert(by_wrapper(std::cref(p), 4) == 104);
    inplace_function<u64(member_probe&&), 16, 8> rvalue = &member_probe::twice;
    fm_assert(rvalue(member_probe{21}) == 42);

    inplace_function<u64&(member_probe&), 16, 8> field = &member_probe::v;
    field(p) = 7;
    fm_assert(p.v == 7);
    inplace_function<u64(const derived_probe*), 16, 8> field_ptr = &member_probe::v;
    fm_assert(field_ptr(&d) == 200);

    inplace_function<u64(const member_probe&, u64), 16, 8> null_member = add_fn{nullptr};
    fm_assert(!null_member);

    return true;
}
static_assert(test_members());

constexpr bool test_conversions()
{
    F none = maybe_empty{false};
    fm_assert(!none);
    F some = maybe_empty{true};
    fm_assert(some(1) == 2);
    some = maybe_empty{false};
    fm_assert(!some);
    F captureless = [](u64 x) noexcept { return x; };
    fm_assert(captureless(5) == 5);
    inplace_function<u64(u64) const, 16, 8> empty_const;
    F from_empty{std::move(empty_const)};
    fm_assert(!from_empty);

    inplace_function<u64(u64), 8, 8> small = [k = u64{13}](u64 x) noexcept { return x * k; };
    inplace_function<u64(u64), 16, 8> bigger{std::move(small)};
    fm_assert(!small);
    fm_assert(bigger(3) == 39);
    small = [](u64 x) noexcept { return x + 2; };
    bigger = std::move(small);
    fm_assert(!small);
    fm_assert(bigger(3) == 5);

    F n = nullptr;
    fm_assert(n == nullptr);
    fm_assert(n.empty());
    F t = triple;
    fm_assert(t != nullptr);
    fm_assert(!t.empty());
    t = nullptr;
    fm_assert(t == nullptr);

    F a = triple;
    F b = [k = u64{5}](u64 x) noexcept { return x + k; };
    a.swap(b);
    fm_assert(a(1) == 6 && b(1) == 3);
    swap(a, b);
    fm_assert(a(1) == 3 && b(1) == 6);
    std::swap(a, b);
    fm_assert(a(1) == 6 && b(1) == 3);
    a.swap(a);
    fm_assert(a(1) == 6);
    F e;
    a.swap(e);
    fm_assert(!a);
    fm_assert(e(1) == 6);

    return true;
}
static_assert(test_conversions());

constexpr bool test_copyable()
{
    i32 alive = 0;
    {
        CF a = counted{&alive, 3};
        CF b = a;
        fm_assert(alive == 2);
        fm_assert(a(5) == 15 && b(5) == 15);
        b = counted{&alive, 4};
        fm_assert(alive == 2);
        a = b;
        fm_assert(alive == 2);
        fm_assert(a(5) == 20);
        CF& alias = a;
        a = alias;
        fm_assert(alive == 2);
        fm_assert(a(5) == 20);
        CF c;
        a = c;
        fm_assert(alive == 1);
        fm_assert(!a);
        c = b;
        fm_assert(alive == 2);
        F m = c;
        fm_assert(alive == 3);
        fm_assert(m(2) == 8);
        F m2 = std::move(c);
        fm_assert(alive == 3);
        fm_assert(!c);
        fm_assert(m2(2) == 8);
    }
    fm_assert(alive == 0);

    CF counter = [sum = u64{0}](u64 x) mutable noexcept { return sum += x; };
    counter(5);
    CF copy = counter;
    fm_assert(counter(1) == 6);
    fm_assert(copy(2) == 7);

    inplace_copyable_function<u64(u64), 16, 8> small = triple;
    CF big = small;
    fm_assert(big(2) == 6);
    fm_assert(small(2) == 6);
    big = nullptr;
    big = small;
    fm_assert(big(3) == 9);

    return true;
}
static_assert(test_copyable());

constexpr bool test_self_replacement()
{
    inplace_function<u64(), 32, 8> a;
    a = successor{[k = u64{41}]() noexcept { return k; }, &a};
    fm_assert(a() == 1);
    fm_assert(a() == 41);

    owned_slot b;
    b = owner{std::make_unique<owned_slot>([]() noexcept { return u64{43}; }), &b};
    fm_assert(b() == 2);
    fm_assert(b() == 43);

    inplace_copyable_function<u64(), 32, 8> c;
    c = copier{[]() noexcept { return u64{47}; }, &c};
    fm_assert(c() == 3);
    fm_assert(c() == 47);

    inplace_function<u64(), 16, 8> d;
    d = converter{value_fn{53}, &d};
    fm_assert(d() == 4);
    fm_assert(d() == 53);

    return true;
}
static_assert(test_self_replacement());

bool inside(const void* p, const void* obj, usize size)
{
    auto x = (uptr)p, o = (uptr)obj;
    return x >= o && x < o + size;
}

void test_inplace()
{
    using P = inplace_function<const void*(), 24, 8>;
    P a = address_probe{};
    fm_assert(inside(a(), &a, sizeof a));
    P b{std::move(a)};
    fm_assert(inside(b(), &b, sizeof b));
    P c;
    c = std::move(b);
    fm_assert(inside(c(), &c, sizeof c));
    c = address_probe{};
    fm_assert(inside(c(), &c, sizeof c));

    inplace_function<const void*(), 40, 8> big{std::move(c)};
    fm_assert(!c);
    fm_assert(inside(big(), &big, sizeof big));
    inplace_copyable_function<const void*(), 24, 8> original = address_probe{};
    inplace_copyable_function<const void*(), 24, 8> copy = original;
    fm_assert(inside(original(), &original, sizeof original));
    fm_assert(inside(copy(), &copy, sizeof copy));

    using A = inplace_function<const void*(), 32, 32>;
    const auto check = [](A& x) {
        const void* p = x();
        fm_assert(inside(p, &x, sizeof x));
        fm_assert((uptr)p % alignof(aligned_probe) == 0);
    };
    A d = aligned_probe{};
    check(d);
    A e{std::move(d)};
    check(e);
    A f;
    f = std::move(e);
    check(f);
}

void test_move_only()
{
    F f = [p = Pointer<u64>{InPlaceInit, u64{4099}}](u64 x) noexcept { return x + *p; };
    F g{std::move(f)};
    fm_assert(g(1) == 4100);

    inplace_function<u64(const Pointer<member_probe>&, u64), 16, 8> by_pointer = &member_probe::add;
    Pointer<member_probe> p{InPlaceInit, member_probe{50}};
    fm_assert(by_pointer(p, 1) == 51);
}

void test_fu2()
{
    F overloaded = fu2::overload([](u64 x) noexcept { return x * 5; }, [](const char*) noexcept { return u64{0}; });
    fm_assert(overloaded(10) == 50);

    const C c = [k = u64{7}](u64 x) noexcept { return x * k; };
    fu2::function_view<u64(u64) const> cv = c;
    fm_assert(cv(3) == 21);

    F f = [k = u64{5}](u64 x) noexcept { return x + k; };
    fu2::function_view<u64(u64)> fv = f;
    fm_assert(fv(3) == 8);

    const C both_c = both_calls{};
    fu2::function_view<u64(u64) const> both_cv = both_c;
    fm_assert(both_cv(10) == 12);
    F both_f = both_calls{};
    fu2::function_view<u64(u64)> both_fv = both_f;
    fm_assert(both_fv(10) == 11);

    Q<u64(u64) volatile> v = qualified_probe{};
    volatile auto& vr = v;
    fu2::function_view<u64(u64) volatile> vv = vr;
    fm_assert(vv(100) == 103);
}

} // namespace

void Test::test_inplace_function()
{
    fm_assert(test_semantics());
    fm_assert(test_qualifiers());
    fm_assert(test_members());
    fm_assert(test_conversions());
    fm_assert(test_copyable());
    fm_assert(test_self_replacement());
    test_inplace();
    test_move_only();
    test_fu2();
}

} // namespace floormat
