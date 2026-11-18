#pragma once
#include <cfloat>

namespace floormat {

template<typename T> struct limits;

template<typename T>
requires (std::is_signed_v<T> && std::is_integral_v<T>)
struct limits<T>
{
    using Type = T;
    static constexpr int digits = int(sizeof(T)*8)-1;
    static constexpr T min{-i64{1} << sizeof(T)*8-1}, max{-(min+1)};
};

template<typename T>
requires (std::is_unsigned_v<T> && std::is_integral_v<T>)
struct limits<T>
{
    using Type = T;
    static constexpr int digits = std::is_same_v<T, bool> ? 1 : int(sizeof(T)*8);
    static constexpr T min{0}, max{T(-1)};
};

template<> struct limits<float>
{
    using Type = float;
    static constexpr int digits = FLT_MANT_DIG;
    static constexpr float max = 2 / FLT_EPSILON, min = -max;
    using integer_type = i32;

    static constexpr float highest = FLT_MAX, lowest = -highest;
    static constexpr float epsilon = FLT_EPSILON;
    static constexpr float normal = FLT_MIN;
};

template<> struct limits<double>
{
    using Type = double;
    static constexpr int digits = DBL_MANT_DIG;
    static constexpr double max = 2 / DBL_EPSILON, min = -max;
    using integer_type = i64;

    static constexpr double highest = DBL_MAX, lowest = -highest;
    static constexpr double epsilon = DBL_EPSILON;
    static constexpr double normal = DBL_MIN;
};

template<> struct limits<long double>
{
    using Type = long double;
    static constexpr int digits = LDBL_MANT_DIG;
    static constexpr long double max = 2 / LDBL_EPSILON, min = -max;

    static constexpr long double highest = LDBL_MAX, lowest = -highest;
    static constexpr long double epsilon = LDBL_EPSILON;
    static constexpr long double normal = LDBL_MIN;
};

} // namespace floormat
