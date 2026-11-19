#include "compat/move.hpp"
#include <utility>

namespace adl_candidates {

struct S {};
int move(S&);
template<typename T> int forward(S&);

} // namespace adl_candidates

namespace floormat {

namespace {

template<typename T> using move_result = decltype(move(std::declval<T&>()));
template<typename T> using forward_result = decltype(forward<T&>(std::declval<T&>()));

using adl_candidates::S;
using P = std::pair<int, int>;

static_assert(std::is_same_v<move_result<S>, S&&>);
static_assert(std::is_same_v<forward_result<S>, S&>);
static_assert(std::is_same_v<move_result<P>, P&&>);
static_assert(std::is_same_v<forward_result<P>, P&>);

} // namespace

} // namespace floormat
