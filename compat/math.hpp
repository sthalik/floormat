#pragma once

#include <cstdlib>
#include <bit>
#include <cmath>

namespace floormat::math::detail {

template<typename T> struct int_type_for_;

template<> struct int_type_for_<float> { using type = int32_t; };
template<> struct int_type_for_<double> { using type = int64_t; };
template<typename T> using int_type_for = typename int_type_for_<T>::type;

} // namespace floormat::math::detail

namespace floormat::math {

template<typename T>
constexpr inline T abs(T x)
requires std::is_arithmetic_v<T>
{
    static_assert(std::is_floating_point_v<T> ||
                  std::is_integral_v<T> && std::is_signed_v<T>);
    return x < T{0} ? -x : x;
}

template <typename T>
requires std::is_arithmetic_v<T>
constexpr inline T sgn(T val)
{
    return T(T{0} < val) - T(val < T{0});
}

template<typename T>
constexpr inline T sqrt(T x0)
requires std::is_floating_point_v<T>
{
    if (std::is_constant_evaluated())
    {
        auto x = x0, prev = T{0};
        while (x != prev)
        {
            prev = x;
            x = T(0.5) * (x + x0 / x);
        }
        return x;
    }
    else
        return std::sqrt(x0);
}

template<typename T>
requires std::is_integral_v<T>
constexpr inline double sqrt(T x)
{
    return sqrt(double(x));
}

template<typename T>
requires std::is_floating_point_v<T>
constexpr inline T ceil(T x)
{
    if (std::is_constant_evaluated())
    {
        using int_ = detail::int_type_for<T>;
        const auto x0 = int_(x);
        return x0 + int_{1} * (x > x0);
    }
    else
        return std::ceil(x);
}

template<typename T>
requires std::is_floating_point_v<T>
constexpr inline T floor(T x)
{
    if (std::is_constant_evaluated())
    {
        using int_ = detail::int_type_for<T>;
        const auto x0 = int_(x);
        return x0 - int_{1} * (x < T{0} && x != x0);
    }
    else
        return std::floor(x);
}

} // namespace floormat::math
