#include "limits.hpp"
#include <climits>
#include <cfloat>

namespace floormat {

namespace {

static_assert(limits<int32_t>::min == -2147483648);
static_assert(limits<int32_t>::max == 2147483647);
static_assert(limits<float>::max == (1 << FLT_MANT_DIG) && limits<float>::min == (-1 << FLT_MANT_DIG));
static_assert(limits<double>::max == (1LL << DBL_MANT_DIG) && limits<double>::min == (-1LL << DBL_MANT_DIG));
static_assert(limits<uint64_t>::min == 0 && limits<uint64_t>::max == (uint64_t)-1);

static_assert(limits<int8_t >::max == INT8_MAX);
static_assert(limits<int16_t>::max == INT16_MAX);
static_assert(limits<int32_t>::max == INT32_MAX);
static_assert(limits<int64_t>::max == INT64_MAX);
static_assert(limits<uint8_t >::max == UINT8_MAX);
static_assert(limits<uint16_t>::max == UINT16_MAX);
static_assert(limits<uint32_t>::max == UINT32_MAX);
static_assert(limits<uint64_t>::max == UINT64_MAX);
static_assert(limits<float>::max == 16777216.f);
static_assert(limits<float>::min == -16777216.f);
static_assert(limits<double>::max == 9007199254740992.);
static_assert(limits<double>::min == -9007199254740992.);

} // namespace


} // namespace floormat
