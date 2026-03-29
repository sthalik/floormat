#include "depth.hpp"

namespace floormat::Depth {

namespace {

constexpr bool check_depth()
{
    fm_assert_equal(value_atʹ({}), (uint32_t)(-most_negative_point).sum());
    fm_assert_not_equal(FLT_MIN, value_at({}));
    return true;
}
static_assert(check_depth());

} // namespace

} // namespace floormat::Depth
