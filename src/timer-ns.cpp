#include "timer.hpp"
#include "compat/assert.hpp"
#include "compat/debug.hpp"
#include <cr/Debug.h>
#include <mg/Functions.h>

namespace floormat {



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
