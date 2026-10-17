#include "app.hpp"
#include "compat/inplace-function.hpp"
#include "compat/function2.hpp"
#include <cr/Pointer.h>
#include <functional>

// Outside the anonymous namespace, where clang warns about their unused members.
namespace floormat::inplace_function_test {

struct move_only_call
{
    move_only_call();
    move_only_call(move_only_call&&) noexcept;
    uint64_t operator()(uint64_t x) const noexcept;
};

struct late_arg;

struct holds_late
{
    inplace_function<uint64_t(late_arg), 8> f;
};

struct late_arg
{
    uint64_t v;
};

struct alignas(16) over_aligned
{
    uint64_t v;
    uint64_t operator()(uint64_t x) const noexcept;
};

struct throwing_move
{
    uint64_t v;
    throwing_move();
    throwing_move(throwing_move&& o) noexcept(false);
    uint64_t operator()(uint64_t x) const noexcept;
};

struct throwing_dtor
{
    uint64_t v;
    ~throwing_dtor() noexcept(false);
    uint64_t operator()(uint64_t x) const noexcept;
};

struct wrong_signature
{
    void operator()(const char*) const noexcept;
};

struct non_const_call
{
    uint64_t operator()(uint64_t x) noexcept;
};

struct const_call_deleted
{
    uint64_t operator()(uint64_t x) noexcept;
    uint64_t operator()(uint64_t x) const noexcept = delete;
};

struct may_throw
{
    uint64_t operator()(uint64_t x) const;
};

struct lvalue_only
{
    uint64_t operator()(uint64_t x) & noexcept;
};

struct returns_value
{
    uint64_t operator()() const noexcept;
};

} // namespace floormat::inplace_function_test

namespace floormat {

namespace detail {
static_assert(!inplace_function<void(), 8, 8>{});
} // namespace detail

namespace {

using namespace inplace_function_test;
using F = inplace_function<uint64_t(uint64_t), 40, 8>;
using C = inplace_function<uint64_t(uint64_t) const, 40, 8>;

template<uint32_t N>
struct sized
{
    uint64_t v[N / 8] = {};
    uint64_t operator()(uint64_t x) const noexcept { return x + v[0]; }
};

struct address_probe
{
    uint64_t pad[3] = {};
    const void* operator()() const noexcept { return this; }
};

struct alignas(32) aligned_probe
{
    uint64_t pad[4] = {};
    const void* operator()() const noexcept { return this; }
};

struct counted
{
    int32_t* alive;
    uint64_t v;

    constexpr counted(int32_t* alive, uint64_t v) noexcept: alive{alive}, v{v} { ++*alive; }
    constexpr counted(const counted& o) noexcept: alive{o.alive}, v{o.v} { ++*alive; }
    constexpr counted(counted&& o) noexcept: alive{o.alive}, v{o.v} { ++*alive; }
    constexpr ~counted() noexcept { --*alive; }
    constexpr uint64_t operator()(uint64_t x) const noexcept { return x * v; }
};

struct mutable_sum
{
    mutable uint64_t sum = 0;
    constexpr uint64_t operator()(uint64_t x) const noexcept { return sum += x; }
};

struct both_calls
{
    constexpr uint64_t operator()(uint64_t x) noexcept { return x + 1; }
    constexpr uint64_t operator()(uint64_t x) const noexcept { return x + 2; }
};

struct by_type
{
    constexpr uint64_t operator()(uint64_t x) const noexcept { return x * 2; }
    constexpr uint64_t operator()(uint32_t x) const noexcept { return x * 3; }
};

struct nodiscard_call
{
    [[nodiscard]] constexpr uint64_t operator()(uint64_t x) const noexcept { return x; }
};

struct no_address
{
    constexpr uint64_t operator()(uint64_t x) const noexcept { return x + 9; }
    void operator&() const = delete;
};

struct qualified_probe
{
    constexpr uint64_t operator()(uint64_t x) & noexcept { return x + 1; }
    constexpr uint64_t operator()(uint64_t x) const& noexcept { return x + 2; }
    constexpr uint64_t operator()(uint64_t x) volatile& noexcept { return x + 3; }
    constexpr uint64_t operator()(uint64_t x) const volatile& noexcept { return x + 4; }
    constexpr uint64_t operator()(uint64_t x) && noexcept { return x + 5; }
    constexpr uint64_t operator()(uint64_t x) const&& noexcept { return x + 6; }
    constexpr uint64_t operator()(uint64_t x) volatile&& noexcept { return x + 7; }
    constexpr uint64_t operator()(uint64_t x) const volatile&& noexcept { return x + 8; }
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
constexpr uint64_t call_probe(uint64_t x)
{
    Q<Signature> f = qualified_probe{};
    return static_cast<Object<Q<Signature>>>(f)(x);
}

struct wide_arg
{
    uint64_t v[3];
};

struct pair_arg
{
    uint32_t a, b;
};

struct moved_arg
{
    uint64_t v;
    constexpr explicit moved_arg(uint64_t v) noexcept: v{v} {}
    constexpr moved_arg(moved_arg&& o) noexcept: v{o.v} { o.v = 0; }
};

struct member_probe
{
    uint64_t v;
    constexpr uint64_t add(uint64_t x) const noexcept { return v + x; }
    constexpr uint64_t twice() && noexcept { return v * 2; }
};

struct derived_probe : member_probe {};

struct maybe_empty
{
    bool full;
    constexpr explicit operator bool() const noexcept { return full; }
    constexpr uint64_t operator()(uint64_t x) const noexcept { return x + 1; }
};

constexpr uint64_t triple(uint64_t x) noexcept { return 3 * x; }

using CF = inplace_copyable_function<uint64_t(uint64_t), 40, 8>;
using add_fn = uint64_t (member_probe::*)(uint64_t) const noexcept;

struct successor
{
    inplace_function<uint64_t(), 8, 8> next;
    inplace_function<uint64_t(), 32, 8>* slot;
    constexpr uint64_t operator()() noexcept
    {
        *slot = std::move(next);
        return 1;
    }
};

using owned_slot = inplace_function<uint64_t(), 16, 8>;

struct owner
{
    std::unique_ptr<owned_slot> next;
    owned_slot* slot;
    constexpr uint64_t operator()() noexcept
    {
        *slot = std::move(*next);
        return 2;
    }
};

struct copier
{
    inplace_copyable_function<uint64_t(), 8, 8> next;
    inplace_copyable_function<uint64_t(), 32, 8>* slot;
    constexpr uint64_t operator()() noexcept
    {
        *slot = next;
        return 3;
    }
};

struct value_fn
{
    uint64_t k;
    constexpr uint64_t operator()() const noexcept { return k; }
};

struct converter
{
    value_fn next;
    inplace_function<uint64_t(), 16, 8>* slot;
    constexpr uint64_t operator()() noexcept
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
static_assert(std::is_constructible_v<inplace_function<uint64_t(uint64_t), 40, 16>, over_aligned>);
static_assert(!std::is_constructible_v<F, throwing_move>);
static_assert(!std::is_constructible_v<F, throwing_dtor>);
static_assert(!std::is_constructible_v<F, wrong_signature>);

static_assert(!std::is_constructible_v<F, fu2::unique_function<uint64_t(uint64_t)>>);
static_assert(!std::is_constructible_v<F, fu2::function_view<uint64_t(uint64_t)>>);
static_assert(std::is_constructible_v<F, inplace_function<uint64_t(uint64_t), 16, 8>>);

static_assert(std::is_constructible_v<F, decltype(triple)&>);
static_assert(std::is_constructible_v<F, uint64_t(*)(uint64_t)>);

static_assert(std::is_invocable_r_v<uint64_t, const C&, uint64_t>);
static_assert(!std::is_invocable_v<const F&, uint64_t>);
static_assert(std::is_constructible_v<F, non_const_call>);
static_assert(!std::is_constructible_v<C, non_const_call>);
static_assert(std::is_constructible_v<F, const_call_deleted>);
static_assert(!std::is_constructible_v<C, const_call_deleted>);
static_assert(std::is_constructible_v<F, inplace_function<uint64_t(uint64_t) const, 16, 8>>);
static_assert(!std::is_constructible_v<C, inplace_function<uint64_t(uint64_t), 16, 8>>);
static_assert(std::is_constructible_v<fu2::function_view<uint64_t(uint64_t) const>, const C&>);
static_assert(std::is_constructible_v<fu2::function_view<uint64_t(uint64_t)>, F&>);

static_assert(std::is_invocable_v<Q<uint64_t(uint64_t)>&&, uint64_t>);
static_assert(std::is_invocable_v<Q<uint64_t(uint64_t) &>&, uint64_t>);
static_assert(!std::is_invocable_v<Q<uint64_t(uint64_t) &>&&, uint64_t>);
static_assert(!std::is_invocable_v<Q<uint64_t(uint64_t) &&>&, uint64_t>);
static_assert(std::is_invocable_v<Q<uint64_t(uint64_t) const&>&&, uint64_t>);
static_assert(!std::is_invocable_v<const Q<uint64_t(uint64_t) const&&>&, uint64_t>);
static_assert(!std::is_invocable_v<const Q<uint64_t(uint64_t) volatile>&, uint64_t>);
static_assert(!std::is_invocable_v<volatile Q<uint64_t(uint64_t) const>&, uint64_t>);
static_assert(std::is_nothrow_invocable_v<Q<uint64_t(uint64_t) noexcept>&, uint64_t>);
static_assert(std::is_nothrow_invocable_v<const volatile Q<uint64_t(uint64_t) const volatile&& noexcept>&&, uint64_t>);
static_assert(!std::is_nothrow_invocable_v<Q<uint64_t(uint64_t)>&, uint64_t>);

static_assert(std::is_constructible_v<Q<uint64_t(uint64_t)>, may_throw>);
static_assert(!std::is_constructible_v<Q<uint64_t(uint64_t) noexcept>, may_throw>);
static_assert(std::is_constructible_v<Q<uint64_t(uint64_t) &>, lvalue_only>);
static_assert(!std::is_constructible_v<Q<uint64_t(uint64_t) &&>, lvalue_only>);
static_assert(!std::is_constructible_v<Q<uint64_t(uint64_t) volatile>, sized<8>>);
static_assert(std::is_constructible_v<Q<uint64_t(uint64_t) volatile>, uint64_t(*)(uint64_t)>);
static_assert(std::is_constructible_v<inplace_function<uint64_t(), 8, 8>, returns_value>);
static_assert(!std::is_constructible_v<inplace_function<const uint64_t&(), 8, 8>, returns_value>);
static_assert(std::is_constructible_v<fu2::function_view<uint64_t(uint64_t) volatile>, volatile Q<uint64_t(uint64_t) volatile>&>);

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
static_assert(std::is_nothrow_constructible_v<inplace_function<uint64_t(uint64_t), 16, 8>, inplace_function<uint64_t(uint64_t), 8, 8>>);
static_assert(std::is_constructible_v<inplace_function<uint64_t(uint64_t), 40, 16>, inplace_function<uint64_t(uint64_t), 16, 8>>);
static_assert(!std::is_constructible_v<inplace_function<uint64_t(uint64_t), 16, 8>, F>);
static_assert(!std::is_constructible_v<F, inplace_function<uint64_t(uint64_t), 16, 16>>);

static_assert(std::is_nothrow_constructible_v<F, std::nullptr_t>);
static_assert(std::is_nothrow_assignable_v<F&, std::nullptr_t>);
static_assert(alignof(inplace_function<void(), 40>) == alignof(void*));
static_assert(sizeof(holds_late) > 0);

static_assert(std::is_constructible_v<inplace_function<uint64_t(const member_probe&, uint64_t) noexcept, 16, 8>, add_fn>);
static_assert(!std::is_constructible_v<inplace_function<uint64_t(member_probe&), 16, 8>, decltype(&member_probe::twice)>);
static_assert(std::is_constructible_v<inplace_function<uint64_t(member_probe&&), 16, 8>, decltype(&member_probe::twice)>);

constexpr F empty_constant;
static_assert(!empty_constant);
constinit F empty_slot;
constexpr F moved_empty = [] { F a; return F{std::move(a)}; }();
static_assert(!moved_empty);
constexpr F reassigned = [] { F a = triple; a = F{}; return a; }();
static_assert(!reassigned);

constexpr bool test_semantics()
{
    F f = [a = uint64_t{7919}, b = uint64_t{104729}](uint64_t x) noexcept { return x * a + b; };
    fm_assert(f);
    fm_assert(f(13) == 13 * 7919 + 104729);

    F g = triple;
    fm_assert(g(17) == 51);
    F h{&triple};
    fm_assert(h(19) == 57);

    inplace_function<uint64_t(uint64_t), 16, 8> small = [k = uint64_t{31}](uint64_t x) noexcept { return x ^ k; };
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

    F counter = [sum = uint64_t{0}](uint64_t x) mutable noexcept { return sum += x; };
    counter(5);
    fm_assert(counter(7) == 12);
    F moved{std::move(counter)};
    fm_assert(moved(1) == 13);

    inplace_function<void(uint64_t&), 8, 8> add = [](uint64_t& x) noexcept { x += 3; return x; };
    uint64_t v = 4;
    add(v);
    fm_assert(v == 7);
    inplace_function<void(uint64_t&), 8, 8> bump = [](uint64_t& x) noexcept { x += 5; };
    bump(v);
    fm_assert(v == 12);
    inplace_function<void(), 8, 8> inc = [p = &v]() noexcept { ++*p; };
    inc();
    fm_assert(v == 13);

    const C c = [k = uint64_t{7}](uint64_t x) noexcept { return x * k; };
    fm_assert(c(3) == 21);
    C sum = mutable_sum{};
    sum(11);
    fm_assert(sum(13) == 24);
    C sum2{std::move(sum)};
    fm_assert(sum2(17) == 41);
    inplace_function<uint64_t(uint64_t) const, 16, 8> small_const = [](uint64_t x) noexcept { return x + 1; };
    F from_const{std::move(small_const)};
    fm_assert(from_const(41) == 42);

    F both_f = both_calls{};
    fm_assert(both_f(10) == 11);
    const C both_c = both_calls{};
    fm_assert(both_c(10) == 12);
    F typed = by_type{};
    fm_assert(typed(10) == 20);
    inplace_function<uint64_t(uint32_t), 8, 8> typed32 = by_type{};
    fm_assert(typed32(10) == 30);

    inplace_function<void(uint64_t), 8, 8> discard = nodiscard_call{};
    discard(1);

    F no_addr = no_address{};
    F no_addr2{std::move(no_addr)};
    fm_assert(no_addr2(1) == 10);

    using fnptr = uint64_t(*)(uint64_t);
    F null_ptr = fnptr{nullptr};
    fm_assert(!null_ptr);
    F reset = triple;
    reset = fnptr{nullptr};
    fm_assert(!reset);

    F target = triple;
    F empty_source;
    target = std::move(empty_source);
    fm_assert(!target);

    using mixed = uint64_t(wide_arg, pair_arg, moved_arg, uint64_t&);
    inplace_function<mixed, 8, 8> args = [](wide_arg w, pair_arg p, moved_arg m, uint64_t& out) noexcept {
        out = m.v;
        return w.v[0] * w.v[1] * w.v[2] + p.a * p.b;
    };
    uint64_t out = 0;
    fm_assert(args(wide_arg{{3, 5, 7}}, pair_arg{11, 13}, moved_arg{1009}, out) == 3 * 5 * 7 + 11 * 13);
    fm_assert(out == 1009);

    holds_late late{[](late_arg a) noexcept { return a.v * 2; }};
    fm_assert(late.f(late_arg{21}) == 42);

    int32_t alive = 0;
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
        c = [](uint64_t x) noexcept { return x; };
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
    fm_assert(call_probe<uint64_t(uint64_t), lv>(100) == 101);
    fm_assert(call_probe<uint64_t(uint64_t), rv>(100) == 101);
    fm_assert(call_probe<uint64_t(uint64_t) const, clv>(100) == 102);
    fm_assert(call_probe<uint64_t(uint64_t) const, rv>(100) == 102);
    fm_assert(call_probe<uint64_t(uint64_t) volatile, vlv>(100) == 103);
    fm_assert(call_probe<uint64_t(uint64_t) const volatile, cvlv>(100) == 104);
    fm_assert(call_probe<uint64_t(uint64_t) &, lv>(100) == 101);
    fm_assert(call_probe<uint64_t(uint64_t) const&, clv>(100) == 102);
    fm_assert(call_probe<uint64_t(uint64_t) const&, rv>(100) == 102);
    fm_assert(call_probe<uint64_t(uint64_t) volatile&, vlv>(100) == 103);
    fm_assert(call_probe<uint64_t(uint64_t) const volatile&, cvlv>(100) == 104);
    fm_assert(call_probe<uint64_t(uint64_t) &&, rv>(100) == 105);
    fm_assert(call_probe<uint64_t(uint64_t) const&&, crv>(100) == 106);
    fm_assert(call_probe<uint64_t(uint64_t) const&&, rv>(100) == 106);
    fm_assert(call_probe<uint64_t(uint64_t) volatile&&, vrv>(100) == 107);
    fm_assert(call_probe<uint64_t(uint64_t) const volatile&&, cvrv>(100) == 108);

    fm_assert(call_probe<uint64_t(uint64_t) noexcept, lv>(100) == 101);
    fm_assert(call_probe<uint64_t(uint64_t) const noexcept, clv>(100) == 102);
    fm_assert(call_probe<uint64_t(uint64_t) volatile noexcept, vlv>(100) == 103);
    fm_assert(call_probe<uint64_t(uint64_t) const volatile noexcept, cvlv>(100) == 104);
    fm_assert(call_probe<uint64_t(uint64_t) & noexcept, lv>(100) == 101);
    fm_assert(call_probe<uint64_t(uint64_t) const& noexcept, clv>(100) == 102);
    fm_assert(call_probe<uint64_t(uint64_t) volatile& noexcept, vlv>(100) == 103);
    fm_assert(call_probe<uint64_t(uint64_t) const volatile& noexcept, cvlv>(100) == 104);
    fm_assert(call_probe<uint64_t(uint64_t) && noexcept, rv>(100) == 105);
    fm_assert(call_probe<uint64_t(uint64_t) const&& noexcept, crv>(100) == 106);
    fm_assert(call_probe<uint64_t(uint64_t) volatile&& noexcept, vrv>(100) == 107);
    fm_assert(call_probe<uint64_t(uint64_t) const volatile&& noexcept, cvrv>(100) == 108);

    Q<uint64_t(uint64_t) volatile> v = qualified_probe{};
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

    inplace_function<uint64_t(const member_probe&, uint64_t) noexcept, 16, 8> by_ref = &member_probe::add;
    fm_assert(by_ref(p, 1) == 101);
    inplace_function<uint64_t(const derived_probe&, uint64_t), 16, 8> by_derived = &member_probe::add;
    fm_assert(by_derived(d, 2) == 202);
    inplace_function<uint64_t(const member_probe*, uint64_t), 16, 8> by_ptr = &member_probe::add;
    fm_assert(by_ptr(&p, 3) == 103);
    inplace_function<uint64_t(std::reference_wrapper<const member_probe>, uint64_t), 16, 8> by_wrapper = &member_probe::add;
    fm_assert(by_wrapper(std::cref(p), 4) == 104);
    inplace_function<uint64_t(member_probe&&), 16, 8> rvalue = &member_probe::twice;
    fm_assert(rvalue(member_probe{21}) == 42);

    inplace_function<uint64_t&(member_probe&), 16, 8> field = &member_probe::v;
    field(p) = 7;
    fm_assert(p.v == 7);
    inplace_function<uint64_t(const derived_probe*), 16, 8> field_ptr = &member_probe::v;
    fm_assert(field_ptr(&d) == 200);

    inplace_function<uint64_t(const member_probe&, uint64_t), 16, 8> null_member = add_fn{nullptr};
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
    F captureless = [](uint64_t x) noexcept { return x; };
    fm_assert(captureless(5) == 5);
    inplace_function<uint64_t(uint64_t) const, 16, 8> empty_const;
    F from_empty{std::move(empty_const)};
    fm_assert(!from_empty);

    inplace_function<uint64_t(uint64_t), 8, 8> small = [k = uint64_t{13}](uint64_t x) noexcept { return x * k; };
    inplace_function<uint64_t(uint64_t), 16, 8> bigger{std::move(small)};
    fm_assert(!small);
    fm_assert(bigger(3) == 39);
    small = [](uint64_t x) noexcept { return x + 2; };
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
    F b = [k = uint64_t{5}](uint64_t x) noexcept { return x + k; };
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
    int32_t alive = 0;
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

    CF counter = [sum = uint64_t{0}](uint64_t x) mutable noexcept { return sum += x; };
    counter(5);
    CF copy = counter;
    fm_assert(counter(1) == 6);
    fm_assert(copy(2) == 7);

    inplace_copyable_function<uint64_t(uint64_t), 16, 8> small = triple;
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
    inplace_function<uint64_t(), 32, 8> a;
    a = successor{[k = uint64_t{41}]() noexcept { return k; }, &a};
    fm_assert(a() == 1);
    fm_assert(a() == 41);

    owned_slot b;
    b = owner{std::make_unique<owned_slot>([]() noexcept { return uint64_t{43}; }), &b};
    fm_assert(b() == 2);
    fm_assert(b() == 43);

    inplace_copyable_function<uint64_t(), 32, 8> c;
    c = copier{[]() noexcept { return uint64_t{47}; }, &c};
    fm_assert(c() == 3);
    fm_assert(c() == 47);

    inplace_function<uint64_t(), 16, 8> d;
    d = converter{value_fn{53}, &d};
    fm_assert(d() == 4);
    fm_assert(d() == 53);

    return true;
}
static_assert(test_self_replacement());

bool inside(const void* p, const void* obj, size_t size)
{
    auto x = (uintptr_t)p, o = (uintptr_t)obj;
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
        fm_assert((uintptr_t)p % alignof(aligned_probe) == 0);
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
    F f = [p = Pointer<uint64_t>{InPlaceInit, uint64_t{4099}}](uint64_t x) noexcept { return x + *p; };
    F g{std::move(f)};
    fm_assert(g(1) == 4100);

    inplace_function<uint64_t(const Pointer<member_probe>&, uint64_t), 16, 8> by_pointer = &member_probe::add;
    Pointer<member_probe> p{InPlaceInit, member_probe{50}};
    fm_assert(by_pointer(p, 1) == 51);
}

void test_fu2()
{
    F overloaded = fu2::overload([](uint64_t x) noexcept { return x * 5; }, [](const char*) noexcept { return uint64_t{0}; });
    fm_assert(overloaded(10) == 50);

    const C c = [k = uint64_t{7}](uint64_t x) noexcept { return x * k; };
    fu2::function_view<uint64_t(uint64_t) const> cv = c;
    fm_assert(cv(3) == 21);

    F f = [k = uint64_t{5}](uint64_t x) noexcept { return x + k; };
    fu2::function_view<uint64_t(uint64_t)> fv = f;
    fm_assert(fv(3) == 8);

    const C both_c = both_calls{};
    fu2::function_view<uint64_t(uint64_t) const> both_cv = both_c;
    fm_assert(both_cv(10) == 12);
    F both_f = both_calls{};
    fu2::function_view<uint64_t(uint64_t)> both_fv = both_f;
    fm_assert(both_fv(10) == 11);

    Q<uint64_t(uint64_t) volatile> v = qualified_probe{};
    volatile auto& vr = v;
    fu2::function_view<uint64_t(uint64_t) volatile> vv = vr;
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
