#pragma once
#include "exception.hpp"
#include "fwd/tuple-fwd.hpp"
#include "limits.hpp"
#include <type_traits>
#include <utility>

namespace floormat {

template<typename To, typename From>
requires (std::is_integral_v<To> && std::is_integral_v<From> ||
          std::is_floating_point_v<To> && std::is_arithmetic_v<From>)
constexpr To checked_cast(From value);

template<typename To, usize N, typename From>
constexpr To checked_cast(const Math::Vector<N, From>& value);

template<typename To, typename... From>
constexpr To checked_cast(const std::tuple<From...>& value);

} // namespace floormat

namespace floormat::detail::checked_cast {

template<typename To, typename From>
[[noreturn]] void throw_out_of_range(From value) noexcept(false)
{
    using T = limits<std::remove_cv_t<To>>;
    fm_throw("checked_cast: {} is out of range [{}, {}]", +value, +T::min, +T::max);
}

template<typename To> struct tuple_cast;

template<typename... To>
struct tuple_cast<std::tuple<To...>>
{
    template<typename... From, usize... I>
    static constexpr std::tuple<To...> cast(const std::tuple<From...>& value, std::index_sequence<I...>);
};

template<typename... To>
template<typename... From, usize... I>
constexpr std::tuple<To...> tuple_cast<std::tuple<To...>>::cast(const std::tuple<From...>& value,
                                                             std::index_sequence<I...>)
{
    static_assert(sizeof...(To) == sizeof...(From));
    return {::floormat::checked_cast<To>(get<I>(value))...};
}

template<typename To, usize N, typename From, usize... I>
constexpr To cast_vector(const Math::Vector<N, From>& value, std::index_sequence<I...>)
{
    static_assert(To::Size == N);
    return To{::floormat::checked_cast<typename To::Type>(value[I])...};
}

} // namespace floormat::detail::checked_cast

namespace floormat {

template<typename To, typename From>
requires (std::is_integral_v<To> && std::is_integral_v<From> ||
          std::is_floating_point_v<To> && std::is_arithmetic_v<From>)
constexpr To checked_cast(From value)
{
    using T = limits<std::remove_cv_t<To>>;
    using F = limits<std::remove_cv_t<From>>;
    if constexpr (std::is_integral_v<To> && std::is_integral_v<From>)
    {
        if constexpr (std::is_signed_v<To> == std::is_signed_v<From>)
        {
            if constexpr (T::digits < F::digits)
            {
                if (value < static_cast<From>(T::min) || value > static_cast<From>(T::max))
                    detail::checked_cast::throw_out_of_range<To>(value);
            }
        }
        else if constexpr (std::is_signed_v<From>)
        {
            if (value < 0)
                detail::checked_cast::throw_out_of_range<To>(value);
            if constexpr (T::digits < F::digits)
            {
                if (static_cast<std::make_unsigned_t<From>>(value) >
                    static_cast<std::make_unsigned_t<From>>(T::max))
                    detail::checked_cast::throw_out_of_range<To>(value);
            }
        }
        else
        {
            if constexpr (T::digits < F::digits)
            {
                if (value > static_cast<std::make_unsigned_t<To>>(T::max))
                    detail::checked_cast::throw_out_of_range<To>(value);
            }
        }
    }
    else if constexpr (std::is_integral_v<From>)
    {
        if constexpr (F::digits > T::digits)
        {
            if (value > static_cast<From>(T::max))
                detail::checked_cast::throw_out_of_range<To>(value);
            if constexpr (std::is_signed_v<From>)
            {
                if (value < static_cast<From>(T::min))
                    detail::checked_cast::throw_out_of_range<To>(value);
            }
        }
    }
    else
    {
        if constexpr (T::digits < F::digits)
        {
            if (value < static_cast<From>(T::min) || value > static_cast<From>(T::max))
                detail::checked_cast::throw_out_of_range<To>(value);
        }
    }
    return static_cast<To>(value);
}

template<typename To, usize N, typename From>
constexpr To checked_cast(const Math::Vector<N, From>& value)
{
    return detail::checked_cast::cast_vector<To>(value, std::make_index_sequence<N>{});
}

template<typename To, typename... From>
constexpr To checked_cast(const std::tuple<From...>& value)
{
    return detail::checked_cast::tuple_cast<To>::cast(value, std::index_sequence_for<From...>{});
}

} // namespace floormat
