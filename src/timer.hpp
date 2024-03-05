#pragma once
#include <compare>

namespace floormat {

struct Ns;

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

#define fm_DATETIME_BUF_SIZE 32
const char* format_datetime_to_string(char(&buf)[fm_DATETIME_BUF_SIZE]);

} // namespace floormat
