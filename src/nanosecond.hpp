#pragma once
#include <compare>

namespace floormat {

struct Ns
{
    explicit constexpr Ns(): stamp{0} {}

    template<typename T> requires (std::is_integral_v<T> && std::is_unsigned_v<T>) explicit constexpr Ns(T x) : stamp{x} {}
    template<typename T> requires (std::is_integral_v<T> && !std::is_unsigned_v<T>) explicit constexpr Ns(T x) : stamp{uint64_t(x)} { fm_assert(x >= T{0}); }

    explicit constexpr operator uint64_t() const { return stamp; }
    explicit constexpr operator double() const = delete;
    explicit constexpr operator float() const = delete;
    friend Ns operator*(const Ns&, const Ns&) = delete;

    friend constexpr Ns operator+(const Ns& lhs, const Ns& rhs);
    friend constexpr Ns operator-(const Ns& lhs, const Ns& rhs);
    template<typename T> requires (std::is_same_v<T, double>) friend Ns operator*(const Ns& lhs, T b);
    template<typename T> requires (std::is_integral_v<T> && std::is_unsigned_v<T>) friend constexpr Ns operator*(const Ns& lhs, T rhs);
    template<typename T> requires (std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) < sizeof(uint64_t)) friend constexpr Ns operator*(const Ns&, T);
    template<typename T> friend constexpr Ns operator*(T lhs, const Ns& rhs);

    friend constexpr uint64_t operator/(const Ns& lhs, const Ns& rhs);
    friend constexpr Ns operator/(const Ns& lhs, uint64_t b);
    friend constexpr uint64_t operator%(const Ns& lhs, const Ns& rhs);

    friend constexpr Ns operator%(const Ns& lhs, uint64_t b);
    friend constexpr Ns& operator+=(Ns& lhs, const Ns& rhs);

    friend constexpr bool operator==(const Ns& lhs, const Ns& rhs) = default;
    friend constexpr std::strong_ordering operator<=>(const Ns& lhs, const Ns& rhs);

    friend Debug& operator<<(Debug& dbg, const Ns& box);

    uint64_t stamp;
};

constexpr inline Ns Minute{60000000000}, Second{1000000000}, Millisecond{1000000}, Microsecond{1000};
constexpr inline Ns Minutes{Minute}, Seconds{Second}, Milliseconds{Millisecond}, Microseconds{Microsecond};

} // namespace floormat
