#include "test/app.hpp"
#include "compat/atomic.hpp"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace floormat::debug::detail {

void emit_assert_fail(const char* expr, const char* file, int line, const char* function)
{
    std::fprintf(stderr, "assertion failed: %s at %s:%d in %s\n", expr, file, line, function);
    std::fflush(stderr);
    std::abort();
}

void emit_abort(const char* file, int line, const char* function, const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, " at %s:%d in %s\n", file, line, function);
    std::fflush(stderr);
    std::abort();
}

void emit_abort() { std::abort(); }

} // namespace floormat::debug::detail

namespace {

template<typename F> void run(const char* name, F f)
{
    auto t0 = std::chrono::steady_clock::now();
    f();
    std::printf("%-20s %10.3f ms\n", name, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    std::fflush(stdout);
}

} // namespace

int main()
{
    std::printf("atomic_wait_sizes:");
    for (auto size : floormat::atomic_wait_sizes)
        std::printf(" %u", (unsigned)size);
    std::printf("\n");
    run("atomic", floormat::Test::test_atomic);
    run("atomic_wait", floormat::Test::test_atomic_wait);
    run("chase_lev_deque", floormat::Test::test_chase_lev_deque);
    run("event_count", floormat::Test::test_event_count);
}
