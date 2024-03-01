#pragma once
#include <compare>

namespace floormat {

struct Ns;

struct Ns
{
    friend Ns operator+(const Ns& lhs, const Ns& rhs);
    friend Ns operator-(const Ns& lhs, const Ns& rhs);
    friend uint64_t operator/(const Ns& lhs, const Ns& rhs);
    friend Ns operator%(const Ns& lhs, const Ns& rhs);

    friend bool operator==(const Ns& lhs, const Ns& rhs);
    friend std::strong_ordering operator<=>(const Ns& lhs, const Ns& rhs);

    explicit operator uint64_t() const;
    explicit operator float() const;
    uint64_t operator*() const;

    uint64_t stamp;
    static constexpr uint64_t Min = 0, Max = (uint64_t)-1;
    static constexpr uint64_t Second = 1000000000, Millisecond = 1000000;

    explicit constexpr Ns(): stamp{0} {}
    explicit constexpr Ns(uint64_t x) : stamp{x} {}
};

struct Time final
{
    static Time now() noexcept;
    bool operator==(const Time&) const noexcept;
    std::strong_ordering operator<=>(const Time&) const noexcept;
    friend Ns operator-(const Time& a, const Time& b) noexcept;
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
