#include "limits.hpp"
#include <cstdint>
#include <cfloat>

namespace floormat {

namespace {

static_assert(limits<i32>::min == -2147483648);
static_assert(limits<i32>::max == 2147483647);
static_assert(limits<float>::max == (1 << FLT_MANT_DIG) && limits<float>::min == (-1 << FLT_MANT_DIG));
static_assert(limits<double>::max == (1LL << DBL_MANT_DIG) && limits<double>::min == (-1LL << DBL_MANT_DIG));
static_assert(limits<u64>::min == 0 && limits<u64>::max == (u64)-1);

static_assert(limits<i8 >::max == INT8_MAX);
static_assert(limits<i16>::max == INT16_MAX);
static_assert(limits<i32>::max == INT32_MAX);
static_assert(limits<i64>::max == INT64_MAX);
static_assert(limits<u8 >::max == UINT8_MAX);
static_assert(limits<u16>::max == UINT16_MAX);
static_assert(limits<u32>::max == UINT32_MAX);
static_assert(limits<u64>::max == UINT64_MAX);
static_assert(limits<float>::max == 16777216.f);
static_assert(limits<float>::min == -16777216.f);
static_assert(limits<double>::max == 9007199254740992.);
static_assert(limits<double>::min == -9007199254740992.);

} // namespace


} // namespace floormat
