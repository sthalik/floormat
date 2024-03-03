#include "timer.hpp"

namespace floormat {

namespace {

constexpr auto MAX = 18446744073709551615u, HALF = 9223372036854775808u;
static_assert(std::is_same_v<uint64_t, std::decay_t<decltype(MAX)>>);
static_assert(std::is_same_v<uint64_t, std::decay_t<decltype(HALF)>>);
static_assert(MAX == (uint64_t)-1);
static_assert(HALF + (HALF - 1) == MAX);
static_assert(MAX - HALF == HALF - 1);
static_assert((HALF-1)*2 == MAX - 1);

// todo!

} // namespace

} // namespace floormat
