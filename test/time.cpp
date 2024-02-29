#include "app.hpp"
#include "src/timer.hpp"
#include <cstdio>
#include <thread>
#include <mg/TimeStl.h>

namespace floormat {

using namespace std::chrono_literals;

void test_app::test_time()
{
#if 0
    constexpr auto to_ms = [](Ns dt) { return Time::to_seconds(dt); };
    Timer::maybe_start();

    Debug{} << "";
    auto t1 = Time::now();
    std::this_thread::sleep_for(8ms);
    auto t2 = Time::now();
    Debug{} << "- foo1" << to_ms(t2 - t1);
    Debug{} << "- foo2" << to_ms(Time::now() - t1);
    Debug{} << "- foo3" << to_ms(Timer::since_epoch());
    std::fputc('\t', stdout);
#endif
}

} // namespace floormat
