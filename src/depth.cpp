#include "depth.hpp"

namespace floormat::Depth {

namespace {

constexpr bool check_depth()
{
    fm_assert_not_equal(FLT_MIN, value_at(-1.f, 0));
    return true;
}
static_assert(check_depth());

} // namespace


} // namespace floormat::Depth
