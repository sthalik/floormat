#include "timer.hpp"
#include "compat/assert.hpp"
#include "compat/debug.hpp"
#include <cr/Debug.h>

namespace floormat {

namespace {

#if 0
constexpr auto MAX = (uint64_t)-1, HALF = MAX/2;

static_assert(MAX - (MAX-0) <= 0);
static_assert(MAX - (MAX-1) <= 1);
static_assert(MAX - (MAX-2) <= 2);

static_assert(HALF + HALF + 1 == MAX);;
static_assert(MAX - HALF <= HALF+1);

//static_assert(MAX - (MAX-1) <= 0); // must fail
//static_assert(MAX - (MAX-2) <= 1); // must fail
//static_assert(MAX - HALF <= HALF); // must fail

#endif

} // namespace

Ns operator+(const Ns& lhs, const Ns& rhs)
{
    constexpr auto max = (uint64_t)-1;
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(max - a <= b);
    return Ns{a + b};
}

Ns operator-(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(a >= b);
    return Ns{a - b};
}

Ns operator*(const Ns& lhs, uint64_t b)
{
    auto a = lhs.stamp;
    auto x = a * b;
    //fm_assert(!(a != 0 && x / a != b));
    fm_assert(a == 0 || x / a == b);
    return Ns{x};
}

Ns operator*(uint64_t a, const Ns& rhs)
{
    auto b = rhs.stamp;
    return Ns{a} * b;
}

uint64_t operator/(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(b != 0);
    return a / b;
}

Ns operator/(const Ns& lhs, uint64_t b)
{
    auto a = lhs.stamp;
    fm_assert(b != 0);
    return Ns{a / b};
}

uint64_t operator%(const Ns& lhs, const Ns& rhs)
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(b != 0);
    return a % b;
}

Ns operator%(const Ns& lhs, uint64_t b)
{
    auto a = lhs.stamp;
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

Debug& operator<<(Debug& dbg, const Ns& box)
{
    auto flags = dbg.flags();
    dbg << "";
    dbg.setFlags(flags | Debug::Flag::NoSpace);
    dbg << fraction((float)((double)box.stamp * 1e-6), 1) << " ms";
    dbg.setFlags(flags);
    return dbg;
}

} // namespace floormat
