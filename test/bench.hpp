#pragma once
#include <chrono>
#include <Corrade/Containers/StringView.h>

namespace floormat {

template<typename F>
requires requires (F& fun) { fun(); }
void bench_run(StringView name, F&& fun)
{
    using namespace std::chrono;
    using clock = high_resolution_clock;
#if 0
    for (int i = 0; i < 20; i++)
        fun();
    const auto t0 = clock::now();
    for (int i = 0; i < 1000; i++)
        fun();
#else
    const auto t0 = clock::now();
    fun();
#endif
    const auto tm = clock::now() - t0;
    Debug{} << "test" << name << "took" << duration_cast<milliseconds>(tm).count() << "ms.";
}

} // namespace floormat
