#pragma once

namespace floormat {

[[noreturn]] inline void unreachable()
{
#if defined __GNUC__
    __builtin_unreachable();
#elif defined _MSC_VER
    __assume(false);
#else
    *(volatile int*)0 = 0;
#endif
}

} // namespace floormat
