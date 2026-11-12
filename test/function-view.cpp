#include "app.hpp"
#include "compat/function-view.hpp"
#include "compat/inplace-function.hpp"

namespace floormat::function_view_test {

struct may_throw
{
    u64 operator()(u64 x) const;
};

struct returns_value
{
    u64 operator()() const noexcept;
};

short to_short(u64 x) noexcept;

} // namespace floormat::function_view_test

namespace floormat {

namespace {

using namespace function_view_test;
using V = function_view<u64(u64) const>;
using VM = function_view<u64(u64)>;
using VN = function_view<u64(u64) noexcept>;
using F = inplace_function<u64(u64), 40, 8>;
using C = inplace_function<u64(u64) const, 40, 8>;
using fnptr = u64(*)(u64);

struct bits
{
    u64 a : 3, b : 61;
};

struct counter
{
    u64 k;
    constexpr u64 get() const noexcept { return k; }
    constexpr u64 add(u64 x) noexcept { return k += x; }
};

struct accumulator
{
    u64 n = 0;
    constexpr u64 operator()(u64 x) noexcept { return n += x; }
};

struct both_calls
{
    constexpr u64 operator()(u64 x) noexcept { return x + 1; }
    constexpr u64 operator()(u64 x) const noexcept { return x + 2; }
};

constexpr u64 triple(u64 x) { return 3 * x; }
constexpr u64 triple_nx(u64 x) noexcept { return 3 * x; }
template<typename T> constexpr T call_with(const function_view<T(T) const>& f, T x) { return f(x); }

static_assert(std::is_trivially_copyable_v<V>);
static_assert(std::is_trivially_copyable_v<function_view<void(bits, counter&, u64) noexcept>>);
static_assert(sizeof(V) == 2 * sizeof(void*));

static_assert(!std::is_constructible_v<V, accumulator&>);
static_assert(std::is_constructible_v<VM, accumulator&>);
static_assert(!std::is_constructible_v<VM, const accumulator&>);
static_assert(!std::is_constructible_v<VN, may_throw&>);
static_assert(std::is_constructible_v<VN, decltype(&triple_nx)>);
static_assert(!std::is_constructible_v<VN, decltype(triple)&>);
static_assert(!std::is_constructible_v<function_view<const u64&()>, returns_value&>);
static_assert(std::is_constructible_v<function_view<u64()>, returns_value&>);
static_assert(!std::is_constructible_v<V, decltype(to_short)&>);
static_assert(!std::is_constructible_v<V, u64>);
static_assert(!std::is_constructible_v<V, volatile both_calls&>);
static_assert(std::is_constructible_v<V, const C&>);
static_assert(std::is_constructible_v<VM, F&>);
static_assert(!std::is_constructible_v<V, const F&>);

constexpr auto times5 = [](u64 x) noexcept { return x * 5; };
constexpr V cx_view = times5;
static_assert(cx_view != nullptr);
static_assert(V{} == nullptr && V{nullptr} == nullptr && !V{fnptr{nullptr}});
static_assert(V{triple} != nullptr);

constexpr bool test_semantics()
{
    u64 k = 7;
    auto add_k = [&k](u64 x) noexcept { return x + k; };
    V a = add_k;
    fm_assert(a(11) == 18);
    fm_assert(call_with(a, u64{11}) == 18);
    k = 13;
    fm_assert(a(11) == 24);

    accumulator acc;
    VM vm = acc;
    vm(17);
    vm(19);
    fm_assert(acc.n == 36);

    both_calls b;
    fm_assert(V{b}(23) == 25);
    fm_assert(VM{b}(23) == 24);
    const both_calls& cb = b;
    fm_assert(VM{cb}(29) == 31);

    fm_assert(V{triple}(31) == 93);
    fm_assert(V{&triple}(37) == 111);
    fm_assert(VN{triple_nx}(41) == 123);

    u64 (counter::*get)() const noexcept = &counter::get;
    counter c{43};
    fm_assert(function_view<u64(const counter&) const>{get}(c) == 43);
    auto add = &counter::add;
    fm_assert(function_view<u64(counter&, u64)>{add}(c, 47) == 90);
    fm_assert(c.k == 90);

    V copy = a;
    a = V{};
    fm_assert(!a);
    fm_assert(copy(53) == 66);

    function_view<void(u64)> discard = add_k;
    discard(59);

    auto take_bits = [](bits x, counter& t, u64 y) noexcept { t.k = x.a + x.b + y; };
    function_view<void(bits, counter&, u64) noexcept> tb = take_bits;
    tb(bits{5, 61}, c, 67);
    fm_assert(c.k == 133);

    V view_of_view = copy;
    fm_assert(view_of_view(71) == 84);
    function_view<u64(u32) const> widened = copy;
    fm_assert(widened(73) == 86);

    const C cc = [m = u64{7}](u64 x) noexcept { return x * m; };
    V cv = cc;
    fm_assert(cv(3) == 21);
    F f = [m = u64{5}](u64 x) noexcept { return x + m; };
    VM fv = f;
    fm_assert(fv(3) == 8);
    const C both_c = both_calls{};
    V both_cv = both_c;
    fm_assert(both_cv(10) == 12);
    F both_f = both_calls{};
    VM both_fv = both_f;
    fm_assert(both_fv(10) == 11);
    const C empty;
    fm_assert(!V{empty});

    return true;
}

#if __cpp_constexpr >= 202306L
static_assert(test_semantics());
static_assert(cx_view(79) == 395);
#endif

} // namespace

void Test::test_function_view()
{
    fm_assert(test_semantics());
    fm_assert(cx_view(79) == 395);
}

} // namespace floormat
