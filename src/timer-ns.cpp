#include "nanosecond.inl"
#include "compat/assert.hpp"
#include "compat/debug.hpp"
#include <cinttypes>
#include <cstdio>
#include <cr/Debug.h>
#include <mg/Functions.h>

namespace floormat {

Debug& operator<<(Debug& dbg, const Ns& val)
{
    const char* unit = "";
    auto x = (double)val.stamp;
    int precision;
    if (val >= 10*Second)
    {
        unit = "s";
        x *= 1e-9;
        precision = 1;
    }
    else if (val >= 1*Second)
    {
        unit = "ₘₛ";
        x *= 1e-6;
        precision = 2;

    }
    else if (val >= 100*Millisecond)
    {
        unit = "ₘₛ";
        x *= 1e-6;
        precision = 3;
    }
    else if (val >= 50*Microsecond)
    {
        unit = "ₘₛ";
        x *= 1e-6;
        precision = 4;
    }
    else if (val >= 1*Microsecond)
    {
        unit = "ₘₛ";
        x *= 1e-3;
        precision = 3;
    }
    else if (val > Ns{100})
    {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%" PRIu64 "_%" PRIu64 " ₙₛ",
                      val.stamp / 1000, val.stamp % 1000);
        return dbg;
    }
    else
    {
        unit = " ₙₛ";
        precision = 0;
    }
    if (!precision)
        dbg << (uint64_t)x << Debug::nospace << unit;
    else
        dbg << fraction((float)x, precision) << Debug::nospace << unit;

    return dbg;
}

} // namespace floormat
