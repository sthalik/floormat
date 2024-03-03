#pragma once
#include "compat/assert.hpp"
#include <compare>

namespace floormat {

struct Ns
{
    explicit constexpr Ns(): stamp{0} {}


    template<typename T>
    requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
    explicit constexpr Ns(T x) : stamp{x} {}

    template<typename T>
    requires (std::is_integral_v<T> && !std::is_unsigned_v<T>)
    explicit constexpr Ns(T x) : stamp{uint64_t(x)} { fm_assert(x >= T{0}); }

#if 0
    template<typename T>
    requires std::is_same_v<T, float>
    explicit constexpr Ns(T x) : stamp{}
    {
        constexpr float max{uint64_t{1} << 24};
        fm_assert(x >= 0);
        fm_assert(x <= max);
        stamp = uint64_t(x);
    }

    template<typename T>
    requires std::is_same_v<T, double>
    explicit constexpr Ns(T x) : stamp{}
    {
        constexpr double max{uint64_t{1} << 54};
        fm_assert(x >= 0);
        fm_assert(x <= max);
        stamp = uint64_t(x);
    }
#endif

    explicit constexpr operator uint64_t() const { return stamp; }

    explicit constexpr operator double() const = delete;
    explicit constexpr operator float() const = delete;

    template<typename T>
    requires (std::is_same_v<T, double>)
    friend Ns operator*(const Ns& lhs, T b)
    {
        constexpr double max{uint64_t{1} << 54};
        auto a = lhs.stamp;
        fm_assert(b >= 0);
        fm_assert(b <= max);
        auto x = double(a) * b;
        fm_assert(x <= max);
        fm_assert(x >= 0);
        return Ns{(uint64_t)x};
    }

#if 0
    template<typename T>
    requires (std::is_same_v<T, double>)
    static Ns from_nonzero(T seconds)
    {
        constexpr double max{uint64_t{1} << 54};
        fm_assert(seconds >= 0);
        fm_assert(seconds <= max);
        return Ns((uint64_t)seconds);
    }
#endif

    static constexpr Ns from_millis(uint64_t a)
    {
        constexpr auto b = uint64_t(1e6);
        const auto x = a * b;
        fm_assert(a == 0 || x / a == b);
        return Ns{x};
    }

    friend constexpr Ns operator+(const Ns& lhs, const Ns& rhs)
    {
        constexpr auto max = (uint64_t)-1;
        auto a = lhs.stamp, b = rhs.stamp;
        fm_assert(max - a >= b);
        return Ns{a + b};
    }

    friend constexpr Ns operator-(const Ns& lhs, const Ns& rhs)
    {
        auto a = lhs.stamp, b = rhs.stamp;
        fm_assert(a >= b);
        return Ns{a - b};
    }

    friend Ns operator*(const Ns&, const Ns&) = delete;

    template<typename T>
    requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
    friend constexpr Ns operator*(const Ns& lhs, T rhs)
    {
        auto a = lhs.stamp, b = uint64_t{rhs};
        auto x = a * b;
        fm_assert(b == 0 || x / b == a);
        return Ns{x};
    }

    template<typename T>
    requires (std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) < sizeof(uint64_t))
    friend constexpr Ns operator*(const Ns& lhs, T rhs)
    {
        fm_assert(rhs >= T{0});
        auto b = uint64_t(rhs);
        auto x = lhs.stamp * b;
        fm_assert(b == 0 || x / b == lhs.stamp);
        return Ns{x};
    }

    template<typename T>
    friend constexpr Ns operator*(T lhs, const Ns& rhs) { return rhs * lhs; }

    template<typename T>
    requires std::is_same_v<float, T>
    friend constexpr Ns operator*(const Ns& lhs, T rhs)
    {
        constexpr float max{uint64_t{1} << 24};
        auto a = lhs.stamp;
        auto x = float(a) * float{rhs};
        fm_assert(x >= 0);
        fm_assert(x <= max);
        return Ns{uint64_t(x)};
    }

#if 0
    template<typename T> requires (!std::is_same_v<Ns, T>)
    friend constexpr Ns operator*(const T& lhs, const Ns& rhs) { return rhs * lhs; }
#endif

    friend constexpr uint64_t operator/(const Ns& lhs, const Ns& rhs)
    {
        auto a = lhs.stamp, b = rhs.stamp;
        fm_assert(b != 0);
        return a / b;
    }

    friend constexpr Ns operator/(const Ns& lhs, uint64_t b)
    {
        auto a = lhs.stamp;
        fm_assert(b != 0);
        return Ns{a / b};
    }

    friend constexpr uint64_t operator%(const Ns& lhs, const Ns& rhs)
    {
        auto a = lhs.stamp, b = rhs.stamp;
        fm_assert(b != 0);
        return a % b;
    }

    friend constexpr Ns operator%(const Ns& lhs, uint64_t b)
    {
        auto a = lhs.stamp;
        fm_assert(b != 0);
        return Ns{a % b};
    }

    friend constexpr Ns& operator+=(Ns& lhs, const Ns& rhs)
    {
        constexpr auto max = (uint64_t)-1;
        auto b = rhs.stamp;
        fm_assert(max - lhs.stamp >= b);
        lhs.stamp += b;
        return lhs;
    }

    friend constexpr bool operator==(const Ns& lhs, const Ns& rhs) = default;

    friend constexpr std::strong_ordering operator<=>(const Ns& lhs, const Ns& rhs)
    {
        auto a = lhs.stamp, b = rhs.stamp;
        return a <=> b;
    }

    friend Debug& operator<<(Debug& dbg, const Ns& box);

    uint64_t stamp;
};

constexpr inline Ns Second{1000000000}, Millisecond{1000000};

struct Time final
{
    static Time now() noexcept;
    bool operator==(const Time&) const noexcept;
    std::strong_ordering operator<=>(const Time&) const noexcept;
    friend Ns operator-(const Time& lhs, const Time& rhs) noexcept;
    [[nodiscard]] Ns update(const Time& ts = now()) & noexcept;

    static float to_seconds(const Ns& ts) noexcept;
    static float to_milliseconds(const Ns& ts) noexcept;

    uint64_t stamp = init();

private:
    static uint64_t init() noexcept;
};

constexpr inline size_t fm_DATETIME_BUF_SIZE = 32;
const char* format_datetime_to_string(char(&buf)[fm_DATETIME_BUF_SIZE]);

} // namespace floormat
