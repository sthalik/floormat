#pragma once
#include <compare>

namespace Magnum::Math { template<class T> class Nanoseconds; }

namespace floormat {

using Ns = Math::Nanoseconds<int64_t>;
//long double operator/(Ns a, Ns b) noexcept;

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

} // namespace floormat
