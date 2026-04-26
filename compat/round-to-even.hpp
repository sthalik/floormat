#pragma once
#include "compat/limits.hpp"
#include <type_traits>
#include <mg/Functions.h>

namespace floormat {

template<typename T>
constexpr T round_to_even(T value, T old_value)
{
    constexpr T hi = T(limits<T>::max & ~T{1});
    constexpr T lo = std::is_unsigned_v<T> ? T{2} : limits<T>::min;
    T even    = T(value & T(~T{1}));
    T rounded = value >= old_value && value < hi ? T(even + T{2}) : even;
    T result  = value & 1 ? rounded : even;
    return Math::clamp(result, lo, hi);
}

} // namespace floormat
