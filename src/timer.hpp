#pragma once
#include <compare>

namespace floormat {

struct Ns;

struct Time final
{
    bool operator==(const Time&) const noexcept;
    std::strong_ordering operator<=>(const Time&) const noexcept;
    friend Ns operator-(const Time& lhs, const Time& rhs) noexcept;
    [[nodiscard]] Ns update(const Time& ts = now()) & noexcept;

    static float to_seconds(const Ns& ts) noexcept;
    static float to_milliseconds(const Ns& ts) noexcept;

    static Time now() noexcept;

    Time() noexcept;
    explicit Time(u64 stamp) noexcept;

    u64 stamp;
};

#define fm_DATETIME_BUF_SIZE 32
const char* format_datetime_to_string(char(&buf)[fm_DATETIME_BUF_SIZE]);

} // namespace floormat
