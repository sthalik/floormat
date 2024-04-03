#pragma once
#include "compat/limits.hpp"
#include "erased-constraints.hpp"
#include <type_traits>
#include <utility>
#include <mg/Vector.h>

namespace floormat::entities::limit_detail {

template<typename T> struct limit_traits;

template<typename T>
requires Math::IsVector<T>::value
struct limit_traits<T>
{
    static constexpr auto min() { return T(limits<typename T::Type>::min); }
    static constexpr auto max() { return T(limits<typename T::Type>::max); }
};

template<typename T>
requires std::is_arithmetic_v<T>
struct limit_traits<T>
{
    static constexpr auto min() { return limits<T>::min; }
    static constexpr auto max() { return limits<T>::max; }
};

template<typename T>
requires std::is_enum_v<T>
struct limit_traits<T>
{
    static constexpr T min() { return T(limits<std::underlying_type_t<T>>::min); }
    static constexpr T max() { return T(limits<std::underlying_type_t<T>>::max); }
};

template<typename T>
struct limit_traits
{
    static_assert(std::is_nothrow_default_constructible_v<T>);
    static constexpr T min() { return T{}; }
    static constexpr T max() { return T{}; }
};

} // namespace floormat::entities::limit_detail


namespace floormat::entities::constraints {

template<typename T> struct range
{
    T min = limit_detail::limit_traits<T>::min();
    T max = limit_detail::limit_traits<T>::max();

    constexpr operator erased_constraints::range() const noexcept;
    constexpr operator std::pair<T, T>() const noexcept;
    constexpr bool operator==(const range&) const noexcept = default;
};

template<typename T> range(T min, T max) -> range<T>;

template<typename T>
requires (std::is_floating_point_v<T>)
constexpr erased_constraints::range erased_range_from_range(T min, T max)
{ return { { .f = min }, { .f = max }, erased_constraints::range::type_::type_float }; }

template<typename T>
requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
constexpr erased_constraints::range erased_range_from_range(T min, T max)
{ return { { .u = min }, { .u = max }, erased_constraints::range::type_::type_uint }; }

template<typename T>
requires (std::is_integral_v<T> && std::is_signed_v<T>)
constexpr erased_constraints::range erased_range_from_range(T min, T max)
{ return { { .i = min }, { .i = max }, erased_constraints::range::type_::type_int }; }

template<size_t N, typename T>
requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
constexpr erased_constraints::range erased_range_from_range(const Math::Vector<N, T>& min0,
                                                            const Math::Vector<N, T>& max0)
{
    static_assert(N <= 4);
    static_assert(sizeof T{} <= sizeof 0uz);
    using type = erased_constraints::range::type_;

    using Element = std::conditional_t<std::is_floating_point_v<T>, float,
                                       std::conditional_t<std::is_unsigned_v<T>, size_t,
                                                          std::make_signed_t<size_t>>>;

    Math::Vector4<Element> min{limits<Element>::min}, max{limits<Element>::max};
    for (auto i = 0u; i < N; i++)
    {
        min[i] = Element(min0[i]);
        max[i] = Element(max0[i]);
    }

    if constexpr(std::is_floating_point_v<T>)
    {
        static_assert(std::is_same_v<T, float>);
        return { .min = {.f4 = min}, .max = {.f4 = max}, .type = type::type_float4 };
    }
    else if constexpr(std::is_unsigned_v<T>)
    {
        static_assert(sizeof T{} <= sizeof 0uz);
        return { .min = {.u4 = min}, .max = {.u4 = max}, .type = type::type_uint4 };
    }
    else if constexpr(std::is_signed_v<T>)
    {
        static_assert(sizeof T{} <= sizeof 0uz);
        return { .min = {.i4 = min}, .max = {.i4 = max}, .type = type::type_int4 };
    }
    else
    {
        static_assert(sizeof T{} == (size_t)-1);
        static_assert(sizeof T{} != (size_t)-1);
        return {};
    }
}

template<typename T>
requires (std::is_enum_v<T>)
constexpr erased_constraints::range erased_range_from_range(T min, T max)
{ return erased_range_from_range(std::underlying_type_t<T>(min), std::underlying_type_t<T>(max)); }

template<typename T>
constexpr range<T>::operator erased_constraints::range() const noexcept
{ return erased_range_from_range(min, max); }

template<typename T> constexpr range<T>::operator std::pair<T, T>() const noexcept { return { min, max }; }

template<> struct range<String> { constexpr operator erased_constraints::range() const noexcept { return {}; } };
template<> struct range<StringView> { constexpr operator erased_constraints::range() const noexcept { return {}; } };

using max_length = erased_constraints::max_length;

} // namespace floormat::entities::constraints
