#pragma once
#include "assert.hpp"
#include "nanosecond.hpp"

namespace floormat {

template<typename T>
requires (std::is_same_v<T, double>)
Ns operator*(const Ns& lhs, T b)
{
    constexpr double max{u64{1} << 53};
    auto a = lhs.stamp;
    fm_assert(b >= 0);
    fm_assert(b <= max);
    auto x = double(a) * b;
    fm_assert(x >= 0);
    fm_assert(x <= max);
    return Ns{(u64)x};
}

constexpr Ns operator+(const Ns& lhs, const Ns& rhs)
{
    constexpr auto max = (u64)-1;
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
    auto a = lhs.stamp, b = u64{rhs};
    auto x = a * b;
    fm_assert(b == 0 || x / b == a);
    return Ns{x};
}

template<typename T>
requires (std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) < sizeof(u64))
constexpr Ns operator*(const Ns& lhs, T rhs)
{
    fm_assert(rhs >= T{0});
    auto b = u64(rhs);
    auto x = lhs.stamp * b;
    fm_assert(b == 0 || x / b == lhs.stamp);
    return Ns{x};
}

template<typename T>
constexpr Ns operator*(T lhs, const Ns& rhs)
{
    return rhs * lhs;
}

constexpr u64 operator/(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(b != 0);
    return a / b;
}

constexpr Ns operator/(const Ns& lhs, u64 b)
{
    auto a = lhs.stamp;
    fm_assert(b != 0);
    return Ns{a / b};
}

constexpr u64 operator%(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(b != 0);
    return a % b;
}

constexpr Ns operator%(const Ns& lhs, u64 b)
{
    auto a = lhs.stamp;
    fm_assert(b != 0);
    return Ns{a % b};
}

constexpr Ns& operator+=(Ns& lhs, const Ns& rhs)
{
    constexpr auto max = (u64)-1;
    auto b = rhs.stamp;
    fm_assert(max - lhs.stamp >= b);
    lhs.stamp += b;
    return lhs;
}

constexpr Ns& operator-=(Ns& lhs, const Ns& rhs)
{
    auto b = rhs.stamp;
    fm_assert(lhs.stamp >= b);
    lhs.stamp -= b;
    return lhs;
}

constexpr std::strong_ordering operator<=>(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    return a <=> b;
}

} // namespace floormat
