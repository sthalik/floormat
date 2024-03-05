#include "nanosecond.inl"
#include "compat/assert.hpp"
#include "compat/debug.hpp"
#include <cr/Debug.h>
#include <mg/Functions.h>

namespace floormat {

Debug& operator<<(Debug& dbg, const Ns& val)
{
    const char* unit;
    double x = val.stamp;
    int precision;
    if (val >= 10*Second)
    {
        unit = "s";
        x *= 1e-9;
        precision = 1;
    }
#if 0
    else if (val >= 1*Second)
    {
        unit = "ms";
        x *= 1e-6;
        precision = 3;

    }
#endif
    else if (val >= 100*Millisecond)
    {
        unit = "ms";
        x *= 1e-6;
        precision = 3;
    }
    else if (val >= 50*Microsecond)
    {
        unit = "ms";
        x *= 1e-6;
        precision = 4;
    }
    else if (val >= 1*Microsecond)
    {
        unit = "usec";
        x *= 1e-3;
        precision = 3;
    }
    else if (val.stamp > 1000)
    {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%llu_%llu ns",
                      val.stamp / 1000, val.stamp % 1000);
        return dbg;
    }
    else
    {
        unit = "ns";
        precision = 0;
    }
    if (!precision)
        dbg << (uint64_t)x << Debug::space << unit;
    else
        dbg << fraction((float)x, precision) << Debug::space << unit;

    return dbg;
}

} // namespace floormat
