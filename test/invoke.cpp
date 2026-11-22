#include "compat/invoke.hpp"
#include <functional>
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

} // namespace

} // namespace floormat
