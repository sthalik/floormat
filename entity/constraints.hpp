#pragma once
#include "erased-constraints.hpp"
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

template<typename T> range(T min, T max) -> range<T>;

template<typename T> constexpr erased_constraints::range erased_range_from_range(T, T)
{ return { {}, {}, erased_constraints::range::type_none }; }

template<typename T>
requires (std::is_floating_point_v<T> && !std::is_integral_v<T>)
constexpr erased_constraints::range erased_range_from_range(T min, T max)
{ return { { .f = min }, { .f = max }, erased_constraints::range::type_::type_float }; }

template<typename T>
requires (std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_floating_point_v<T>)
constexpr erased_constraints::range erased_range_from_range(T min, T max)
{ return { { .u = min }, { .u = max }, erased_constraints::range::type_::type_uint }; }

template<typename T>
requires (std::is_integral_v<T> && std::is_signed_v<T> && !std::is_floating_point_v<T>)
constexpr erased_constraints::range erased_range_from_range(T min, T max)
{ return { { .i = min }, { .i = max }, erased_constraints::range::type_::type_int }; }

template<typename T>
constexpr range<T>::operator erased_constraints::range() const noexcept
{ return erased_range_from_range(min, max); }

template<typename T> constexpr range<T>::operator std::pair<T, T>() const noexcept { return { min, max }; }

template<> struct range<String>
{
    constexpr operator erased_constraints::range() const noexcept { return {}; }
};

using max_length = erased_constraints::max_length;

} // namespace floormat::entities::constraints
