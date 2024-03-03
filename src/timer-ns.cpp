#include "timer.hpp"
#include "compat/assert.hpp"
#include "compat/debug.hpp"
#include <cr/Debug.h>
#include <mg/Functions.h>

namespace floormat {

namespace {

constexpr auto MAX = (uint64_t)-1, HALF = MAX/2;

static_assert(MAX - (MAX-0) <= 0);
static_assert(MAX - (MAX-1) <= 1);
static_assert(MAX - (MAX-2) <= 2);

static_assert(HALF + HALF + 1 == MAX);;
static_assert(MAX - HALF <= HALF+1);

//static_assert(MAX - (MAX-1) <= 0); // must fail
//static_assert(MAX - (MAX-2) <= 1); // must fail
//static_assert(MAX - HALF <= HALF); // must fail

} // namespace

Debug& operator<<(Debug& dbg, const Ns& box)
{
    const auto value = (float)((double)box.stamp * 1e-6);
    const auto absval = Math::abs(value);
    int precision;
    if (absval < 2)
        precision = 4;
    else if (absval < 5)
        precision = 2;
    else if (absval < 100)
        precision = 1;
    else
        precision = 0;
    auto flags = dbg.flags();
    dbg << "";
    dbg.setFlags(flags | Debug::Flag::NoSpace);
    //dbg << "{";
    dbg << fraction(value, precision);
    dbg << " ms";
    //dbg << "}";
    dbg.setFlags(flags);
    return dbg;
}

} // namespace floormat
