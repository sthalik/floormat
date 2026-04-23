#include "pass-through.hpp"
#include "compat/array-size.hpp"
#include "compat/assert.hpp"
#include <array>

namespace floormat {

namespace {

constexpr auto make_walk_through_mask() noexcept
{
    pass_through_mask array;
    array[(unsigned)pass_mode::blocked] = false;
    array[(unsigned)pass_mode::pass] = true;
    array[(unsigned)pass_mode::see_through] = false;
    array[(unsigned)pass_mode::shoot_through] = false;
    return array;
}

constexpr auto make_see_through_mask() noexcept
{
    pass_through_mask array;
    array[(unsigned)pass_mode::blocked] = false;
    array[(unsigned)pass_mode::pass] = true;
    array[(unsigned)pass_mode::see_through] = true;
    array[(unsigned)pass_mode::shoot_through] = true;
    return array;
}

constexpr auto make_shoot_through_mask() noexcept
{
    pass_through_mask array;
    array[(unsigned)pass_mode::blocked] = false;
    array[(unsigned)pass_mode::pass] = true;
    array[(unsigned)pass_mode::see_through] = false;
    array[(unsigned)pass_mode::shoot_through] = true;
    return array;
}

constexpr auto make_not_blocked_mask() noexcept
{
    pass_through_mask array;
    array[(unsigned)pass_mode::blocked] = false;
    array[(unsigned)pass_mode::pass] = true;
    array[(unsigned)pass_mode::see_through] = true;
    array[(unsigned)pass_mode::shoot_through] = true;
    return array;
}

} // namespace

#if 0
bool can_walk_through(pass_mode p)
{
    constexpr auto array = make_walk_through_mask();
    fm_assert((unsigned)p < array_size(array));
    return array.data()[(unsigned)p];
}

bool can_see_through(pass_mode p)
{
    constexpr auto array = make_see_through_mask();
    fm_assert((unsigned)p < array_size(array));
    return array.data()[(unsigned)p];
}

bool can_shoot_through(pass_mode p)
{
    constexpr auto array = make_shoot_through_mask();
    fm_assert((unsigned)p < array_size(array));
    return array.data()[(unsigned)p];
}
#endif

bool can_pass_through_mask(pass_through_mask mask, pass_mode pʹ)
{
    auto p = (unsigned)pʹ;
    fm_assert(p < array_size(mask));
    return mask.data()[p];
}

const pass_through_mask can_walk_through_mask  = make_walk_through_mask();
const pass_through_mask can_see_through_mask   = make_see_through_mask();
const pass_through_mask can_shoot_through_mask = make_shoot_through_mask();
const pass_through_mask not_blocked_pass_through_mask = make_not_blocked_mask();


} // namespace floormat
