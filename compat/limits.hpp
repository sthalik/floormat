#pragma once
#include <cfloat>

namespace floormat {

template<typename T> struct limits;

template<typename T>
requires (std::is_signed_v<T> && std::is_integral_v<T>)
struct limits<T>
{
    using Type = T;
    static constexpr T min{-int64_t{1} << sizeof(T)*8-1}, max{-(min+1)};
};

template<typename T>
requires (std::is_unsigned_v<T> && std::is_integral_v<T>)
struct limits<T>
{
    using Type = T;
    static constexpr T min{0}, max{T(-1)};
};

template<> struct limits<float>
{
    using Type = float;
    static constexpr float max{1 << 24}, min{-max};
    using integer_type = int32_t;

    static constexpr float epsilon = FLT_EPSILON;
    static constexpr float normal = FLT_MIN;
};

template<> struct limits<double>
{
    using Type = double;
    static constexpr double max = double{uint64_t{1} << 53}, min{-max};
    using integer_type = int64_t;

    static constexpr double epsilon = DBL_EPSILON;
    static constexpr double normal = DBL_MIN;
};

} // namespace floormat
