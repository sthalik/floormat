#include "depth.hpp"
#include "compat/limits.hpp"

namespace floormat::Depth {

namespace {

constexpr bool check_depth()
{
    constexpr u32 max_offset = 1 << 17;
    fm_assert(value_atʹ(point{most_negative_point}) > max_offset);
    fm_assert(value_atʹ(point{most_positive_point}) < limits<u32>::max - max_offset);
    return true;
}
static_assert(check_depth());

} // namespace


} // namespace floormat::Depth
