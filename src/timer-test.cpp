#include "timer.hpp"

namespace floormat {

namespace {

constexpr uint64_t MAX{18446744073709551615ULL} , HALF{9223372036854775808ULL};
static_assert(sizeof MAX == 8);
static_assert(MAX == (uint64_t)-1);
static_assert(HALF + (HALF - 1) == MAX);
static_assert(MAX - HALF == HALF - 1);
static_assert((HALF-1)*2 == MAX - 1);

// todo!

} // namespace

} // namespace floormat
