#pragma once

#include <bit>
#include <cmath>

namespace floormat::math::detail {

constexpr double sqrt_newton_raphson(double x, double curr, double prev)
{
    return curr == prev
           ? curr
           : sqrt_newton_raphson(x, 0.5 * (curr + x / curr), curr);
}

template<typename T> requires std::is_floating_point_v<T> struct float_constants;

template<>
struct float_constants<double>
{
    static constexpr auto quiet_nan = std::bit_cast<double>(uint64_t(0x7FF8000000000000ULL));
    static constexpr auto positive_infinity = std::bit_cast<double>(uint64_t(0x7FF0000000000000ULL));
    static constexpr auto negative_infinity = std::bit_cast<double>(uint64_t(0xFFF0000000000000ULL));
};

template<>
struct float_constants<float>
{
    static constexpr auto quiet_nan = std::bit_cast<float>(uint32_t(0x7FC00000U));
    static constexpr auto positive_infinity = std::bit_cast<float>(uint32_t(0x7F800000U));
    static constexpr auto negative_infinity = std::bit_cast<float>(uint32_t(0xFF800000U));
};

} // namespace floormat::math::detail

namespace floormat::math {

template<typename T>
constexpr inline T sqrt(T x)
requires std::is_floating_point_v<T>
{
    if (std::is_constant_evaluated())
    {
        using K = detail::float_constants<T>;
        return x >= 0 && x < K::positive_infinity
               ? T(detail::sqrt_newton_raphson(double(x), double(x), 0))
               : K::quiet_nan;
    }
    else
        return std::sqrt(x);
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
        const auto x0 = int64_t(x);
        if (x > x0)
            return T(x0 + int64_t(1));
        else
            return x0;
    }
    else
        return std::ceil(x);
}

} // namespace floormat::math
