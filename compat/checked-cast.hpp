#pragma once
#include "limits.hpp"
#include "sequence-traits.hpp"
#include <type_traits>

namespace floormat {

template<typename To, typename From>
requires (std::is_integral_v<To> && std::is_integral_v<From> ||
          std::is_floating_point_v<To> && std::is_arithmetic_v<From>)
constexpr To checked_cast(From value);

template<random_access_sequence To, random_access_sequence From>
requires (sequence_traits<std::remove_cv_t<To>>::size == sequence_traits<std::remove_cv_t<From>>::size)
constexpr To checked_cast(const From& value);

} // namespace floormat

namespace floormat::detail::checked_cast {

struct erased_number
{
    enum type_ : unsigned char { type_int, type_uint, type_float, };
    union { imax i; umax u; long double f; };
    type_ type;

    template<typename T> requires (std::is_signed_v<T> && std::is_integral_v<T>)
    erased_number(T value) : i{value}, type{type_int} {}

    template<typename T> requires (std::is_unsigned_v<T> && std::is_integral_v<T>)
    erased_number(T value) : u{value}, type{type_uint} {}

    template<typename T> requires std::is_floating_point_v<T>
    erased_number(T value) : f{static_cast<long double>(value)}, type{type_float} {}
};

[[noreturn]] void throw_out_of_range(erased_number value, erased_number min, erased_number max) noexcept(false);

template<typename To, typename From>
[[noreturn]] void throw_out_of_range(From value) noexcept(false)
{
    using T = limits<std::remove_cv_t<To>>;
    throw_out_of_range(value, T::min, T::max);
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

template<random_access_sequence To, random_access_sequence From>
requires (sequence_traits<std::remove_cv_t<To>>::size == sequence_traits<std::remove_cv_t<From>>::size)
constexpr To checked_cast(const From& value)
{
    return transform_elements<To>(value, [](auto to, const auto& x) {
        return ::floormat::checked_cast<typename decltype(to)::type>(x);
    });
}

} // namespace floormat
