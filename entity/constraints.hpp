#pragma once
#include "erased-constraints.hpp"
#include <type_traits>
#include <limits>
#include <utility>
#include <mg/Vector.h>

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
    using limits = std::numeric_limits<T>;
    using type = erased_constraints::range::type_;

    using Element = std::conditional_t<std::is_floating_point_v<T>, float,
                                       std::conditional_t<std::is_unsigned_v<T>, size_t,
                                                          std::make_signed_t<size_t>>>;

    Math::Vector4<Element> min{limits::min()}, max{limits::max()};
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
