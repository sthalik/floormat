#include "compat/invoke.hpp"
#include <functional>
#include <tuple>
#include <utility>

namespace adl_candidates {

struct S {};
int invoke(long (*)(S&), S&);

} // namespace adl_candidates

namespace floormat {

namespace {

template<typename F, typename T> using invoke_result = decltype(invoke(std::declval<F>(), std::declval<T&>()));

using adl_candidates::S;
using P = std::pair<int, int>;

static_assert(std::is_same_v<invoke_result<long (*)(S&), S>, long>);
static_assert(std::is_same_v<invoke_result<long (*)(P&), P>, long>);

struct item
{
    int value;
    constexpr int plus(int x) const noexcept { return value + x; }
    constexpr int take() && { return value; }
};

template<typename... Ts> concept is_invocable = requires(Ts&&... xs) { invoke(static_cast<Ts&&>(xs)...); };
template<typename... Ts> concept is_same_as_std = std::is_invocable_v<Ts...> == is_invocable<Ts...>;
template<typename... Ts> using result = decltype(invoke(std::declval<Ts>()...));

static_assert(is_invocable<int item::*, item&>);
static_assert(!is_invocable<int item::*, item&, int>);
static_assert(!is_invocable<int item::*>);
static_assert(!is_invocable<int item::*, int&>);
static_assert(is_invocable<int (item::*)(int) const noexcept, const item*, int>);
static_assert(!is_invocable<int (item::*)(int) const noexcept, const item&>);
static_assert(!is_invocable<int (item::*)(int) const noexcept, const item&, int, int>);
static_assert(is_invocable<int (item::*)() &&, item>);
static_assert(!is_invocable<int (item::*)() &&, item&>);
static_assert(!is_invocable<int (*)(int), int, int>);
static_assert(is_same_as_std<int item::*, item&, int> && is_same_as_std<int item::*, std::reference_wrapper<item>>);
static_assert(is_same_as_std<int (item::*)() &&, item&> && is_same_as_std<int (item::*)(int) const noexcept, item*>);

static_assert(std::is_same_v<result<int item::*, item&>, int&>);
static_assert(std::is_same_v<result<int item::*, item>, int&&>);
static_assert(std::is_same_v<result<int item::*, const item*>, const int&>);
static_assert(std::is_same_v<result<int item::*, std::reference_wrapper<item>>, int&>);
static_assert(std::is_same_v<result<int item::*, const item&>, std::invoke_result_t<int item::*, const item&>>);
static_assert(std::is_same_v<result<int item::*, item>, std::invoke_result_t<int item::*, item>>);
static_assert(noexcept(invoke(&item::plus, std::declval<const item&>(), 1)));
static_assert(!noexcept(invoke(&item::take, std::declval<item>())));

constexpr bool test_write_through_member()
{
    item x{3};
    invoke(&item::value, x) = 5;
    return x.value == 5 && invoke(&item::plus, &x, 2) == 7 && invoke(&item::take, item{11}) == 11;
}
static_assert(test_write_through_member());
static_assert(std::apply(invoke, std::tuple{&item::plus, item{1}, 2}) == 3);

} // namespace

} // namespace floormat
