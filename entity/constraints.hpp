#pragma once
#include "erased-constraints.hpp"
#include <cstddef>
#include <type_traits>
#include <limits>
#include <utility>
#include <Corrade/Containers/StringView.h>

namespace floormat::entities::constraints {

template<typename T> struct range
{
    using limits = std::numeric_limits<T>;
    T min = limits::min(), max = limits::max();

    constexpr operator erased_constraints::range() const noexcept;
    constexpr operator std::pair<T, T>() const noexcept;
    constexpr bool operator==(const range&) const noexcept = default;
};

template<typename T>
constexpr range<T>::operator erased_constraints::range() const noexcept
{
    using enum erased_constraints::range::type_;
    if constexpr (std::is_floating_point_v<T>)
        return { { .f = min }, { .f = max }, type_float };
    if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
        return { {.u = min}, {.u = max}, type_uint };
    if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
        return { {.i = min}, {.i = max}, type_int };
    return { {}, {}, type_none };
}

template<typename T> constexpr range<T>::operator std::pair<T, T>() const noexcept { return { min, max }; }

using max_length = erased_constraints::max_length;
using group = erased_constraints::group;

} // namespace floormat::entities::constraints
