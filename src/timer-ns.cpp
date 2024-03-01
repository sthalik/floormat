#include "timer.hpp"
#include "compat/assert.hpp"

namespace floormat {

Ns operator+(const Ns& lhs, const Ns& rhs)
{
    constexpr auto max = (uint64_t)-1;
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(max - a < b);
    return Ns{a + b};
}

Ns operator-(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(a >= b);
    return Ns{a - b};
}

uint64_t operator/(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(b != 0);
    return a / b;
}

Ns operator%(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(b != 0);
    return Ns{a % b};
}

bool operator==(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    return a == b;
}

std::strong_ordering operator<=>(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    return a <=> b;
}

Ns::operator uint64_t() const { return stamp; }
Ns::operator float() const { return float(stamp); }
uint64_t Ns::operator*() const { return stamp; }
Ns::Ns() : stamp{0} {}
Ns::Ns(uint64_t x) : stamp{x} {}

} // namespace floormat
