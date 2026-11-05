#include "app.hpp"
#include "compat/nanosecond.inl"
#include "compat/timer.hpp"

namespace floormat {

void Test::test_timer()
{
    // Time() and Time::now() count from the same epoch.
    const Time t0;
    const auto t1 = Time::now();
    fm_assert(t0 <= t1);
    fm_assert(t1 - t0 < Second);
}

} // namespace floormat
