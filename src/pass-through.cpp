#include "pass-through.hpp"
#include "compat/array-size.hpp"
#include "compat/assert.hpp"
#include <array>

namespace floormat {

bool can_walk_through(pass_mode p)
{
    constexpr auto array = [] {
        std::array<bool, (size_t)pass_mode::COUNT> array {};
        array[(unsigned)pass_mode::blocked] = false;
        array[(unsigned)pass_mode::pass] = true;
        array[(unsigned)pass_mode::see_through] = false;
        array[(unsigned)pass_mode::shoot_through] = false;
        return array;
    }();
    fm_assert((unsigned)p < array_size(array));
    return array.data()[(unsigned)p];
}

bool can_see_through(pass_mode p)
{
    constexpr auto array = [] {
        std::array<bool, (size_t)pass_mode::COUNT> array {};
        array[(unsigned)pass_mode::blocked] = false;
        array[(unsigned)pass_mode::pass] = true;
        array[(unsigned)pass_mode::see_through] = true;
        array[(unsigned)pass_mode::shoot_through] = true;
        return array;
    }();
    fm_assert((unsigned)p < array_size(array));
    return array.data()[(unsigned)p];
}

bool can_shoot_through(pass_mode p)
{
    constexpr auto array = [] {
        std::array<bool, (size_t)pass_mode::COUNT> array {};
        array[(unsigned)pass_mode::blocked] = false;
        array[(unsigned)pass_mode::pass] = true;
        array[(unsigned)pass_mode::see_through] = false;
        array[(unsigned)pass_mode::shoot_through] = true;
        return array;
    }();
    fm_assert((unsigned)p < array_size(array));
    return array.data()[(unsigned)p];
}

} // namespace floormat
