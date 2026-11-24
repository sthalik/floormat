#pragma once
#include "limits.hpp"
#include "sequence-traits.hpp"
#include <mg/Functions.h>

namespace floormat {

template<typename To, typename From>
requires (std::is_integral_v<To> && std::is_integral_v<From> ||
          std::is_floating_point_v<To> && std::is_arithmetic_v<From>)
constexpr To saturate_cast(From value)
{
    using T = limits<std::remove_cv_t<To>>;
    using F = limits<std::remove_cv_t<From>>;
    if constexpr (std::is_signed_v<From> && std::is_integral_v<To> && !std::is_signed_v<To>)
    {
        if constexpr(T::digits < F::digits)
            return To(Math::clamp(value, From{0}, static_cast<From>(T::max)));
        else
            return To(Math::max(value, From{0}));
    }
    else if constexpr (T::digits >= F::digits)
        return To(value);
    else if constexpr (std::is_signed_v<From>)
        return To(Math::clamp(value, static_cast<From>(T::min), static_cast<From>(T::max)));
    else
        return To(Math::min(value, static_cast<From>(T::max)));
}

template<random_access_sequence To, random_access_sequence From>
requires (sequence_traits<std::remove_cv_t<To>>::size == sequence_traits<std::remove_cv_t<From>>::size)
constexpr To saturate_cast(const From& value)
{
    return transform_elements<To>(value, [](auto to, const auto& x) {
        return ::floormat::saturate_cast<typename decltype(to)::type>(x);
    });
}

} // namespace floormat
