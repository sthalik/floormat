#pragma once
#include "compat/assert.hpp"
#include "nanosecond.hpp"

namespace floormat {

template<typename T>
requires (std::is_same_v<T, double>)
Ns operator*(const Ns& lhs, T b)
{
    constexpr double max{uint64_t{1} << 53};
    auto a = lhs.stamp;
    fm_assert(b >= 0);
    fm_assert(b <= max);
    auto x = double(a) * b;
    fm_assert(x >= 0);
    fm_assert(x <= max);
    return Ns{(uint64_t)x};
}

constexpr Ns operator+(const Ns& lhs, const Ns& rhs)
{
    constexpr auto max = (uint64_t)-1;
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(max - a >= b);
    return Ns{a + b};
}

constexpr Ns operator-(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(a >= b);
    return Ns{a - b};
}

template<typename T>
requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
constexpr Ns operator*(const Ns& lhs, T rhs)
{
    auto a = lhs.stamp, b = uint64_t{rhs};
    auto x = a * b;
    fm_assert(b == 0 || x / b == a);
    return Ns{x};
}

template<typename T>
requires (std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) < sizeof(uint64_t))
constexpr Ns operator*(const Ns& lhs, T rhs)
{
    fm_assert(rhs >= T{0});
    auto b = uint64_t(rhs);
    auto x = lhs.stamp * b;
    fm_assert(b == 0 || x / b == lhs.stamp);
    return Ns{x};
}

template<typename T>
constexpr Ns operator*(T lhs, const Ns& rhs)
{
    return rhs * lhs;
}

template<typename T>
requires std::is_same_v<float, T>
constexpr Ns operator*(const Ns& lhs, T rhs)
{
    constexpr float max{uint64_t{1} << 24};
    auto a = lhs.stamp;
    auto x = float(a) * float{rhs};
    fm_assert(x >= 0);
    fm_assert(x <= max);
    return Ns{uint64_t(x)};
}

constexpr uint64_t operator/(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(b != 0);
    return a / b;
}

constexpr Ns operator/(const Ns& lhs, uint64_t b)
{
    auto a = lhs.stamp;
    fm_assert(b != 0);
    return Ns{a / b};
}

constexpr uint64_t operator%(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(b != 0);
    return a % b;
}

constexpr Ns operator%(const Ns& lhs, uint64_t b)
{
    auto a = lhs.stamp;
    fm_assert(b != 0);
    return Ns{a % b};
}

constexpr Ns& operator+=(Ns& lhs, const Ns& rhs)
{
    constexpr auto max = (uint64_t)-1;
    auto b = rhs.stamp;
    fm_assert(max - lhs.stamp >= b);
    lhs.stamp += b;
    return lhs;
}

constexpr std::strong_ordering operator<=>(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    return a <=> b;
}

} // namespace floormat
