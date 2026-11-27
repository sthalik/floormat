#include "timer.hpp"
#include "assert.hpp"
#include "array-size.hpp"
#include "nanosecond.hpp"
#include "stl/chrono.hpp"
#include <ctime>
#include <compare>
#include <cstdio>
#include <mg/Time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <time.h>
#endif

namespace floormat {

using std::chrono::duration_cast;
using std::chrono::duration;

// Not high_resolution_clock: libstdc++ aliases it to system_clock, which is settable, and
// Time::now() subtracts an epoch into an unsigned so a backwards step wraps.
using Clock = std::chrono::steady_clock;
static_assert(Clock::is_steady);
using SystemClock = std::chrono::system_clock;
using Nsecs = duration<u64, std::nano>;
using Millis = duration<u64, std::milli>; // 32-bit rep would truncate ms-since-epoch before the %1000

namespace {
u64 get_time() noexcept { return duration_cast<Nsecs>(Clock::now().time_since_epoch()).count(); }
const u64 Epoch = get_time();

#ifdef _WIN32
struct waitable_timer
{
    HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

    ~waitable_timer() noexcept
    {
        if (handle)
            CloseHandle(handle);
    }
};
#endif
} // namespace

Time::Time() noexcept: Time{now()} {}
Time::Time(u64 stamp) noexcept: stamp{stamp} {}

Time Time::now() noexcept
{
    auto val = get_time();
    auto ret = val - Epoch;
    return Time{ret};
}

Ns Time::update(const Time& ts) & noexcept
{
    auto ret = ts - *this;
    stamp = ts.stamp;
    return ret;
}

bool Time::operator==(const Time&) const noexcept = default;
std::strong_ordering Time::operator<=>(const Time&) const noexcept = default;

float Time::to_seconds(const Ns ts) noexcept
{
    if (ts.stamp == 0) [[unlikely]]
        return 0;
    else
    {
        auto x = double(ts.stamp) * 1e-9;
        fm_assert(x <= double{1 << 24});
        //fm_assert(x >= 1e-10f);
        return (float)x;
    }
}

float Time::to_milliseconds(const Ns ts) noexcept
{
    if (ts.stamp == 0) [[unlikely]]
        return 0;
    else
    {
        auto x = double(ts.stamp) * 1e-6;
        fm_assert(x <= double{1 << 24});
        fm_assert(x >= 1e-10);
        return (float)x;
    }
}

void sleep(Ns sleep_duration) noexcept
{
#ifdef _WIN32
    const u64 ms = sleep_duration.stamp / Millisecond.stamp + (sleep_duration.stamp % Millisecond.stamp != 0);
    fm_assert(ms < INFINITE);
    thread_local const waitable_timer timer;
    u64 now = Time::now().stamp;
    const u64 deadline = now + sleep_duration.stamp;
    while (now < deadline)
    {
        const u64 remaining = deadline - now;
        if (timer.handle)
        {
            LARGE_INTEGER duration;
            duration.QuadPart = -(LONGLONG)((remaining + 99) / 100);
            bool ok = SetWaitableTimer(timer.handle, &duration, 0, nullptr, nullptr, FALSE);
            fm_assert(ok);
            ok = WaitForSingleObject(timer.handle, INFINITE) == WAIT_OBJECT_0;
            fm_assert(ok);
        }
        else
            Sleep((DWORD)(remaining / Millisecond.stamp + (remaining % Millisecond.stamp != 0)));
        now = Time::now().stamp;
    }
#else
    timespec remaining = {
        .tv_sec = (time_t)(sleep_duration.stamp / Second.stamp),
        .tv_nsec = (long)(sleep_duration.stamp % Second.stamp),
    };
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR)
        continue;
#endif
}

const char* format_datetime_to_string(char (&buf)[fm_DATETIME_BUF_SIZE])
{
    constexpr const char* fmt = "%a, %d %b %Y %H:%M:%S.";
    constexpr usize fmtsize = array_size("Thu 01 Mon 197000 00:00:00.");
    static_assert(array_size(buf) - fmtsize == 4);
    const auto t    = SystemClock::now();
    const auto ms   = duration_cast<Millis>(t.time_since_epoch()) % 1000;
    const auto time = SystemClock::to_time_t(t);
    const auto* tm  = std::localtime(&time);
    auto len = std::strftime(buf, array_size(buf), fmt, tm);
    fm_assert(len > 0 && len <= fmtsize);
    auto len2 = std::sprintf(buf + len, "%03u", (unsigned)ms.count());
    fm_assert(len2 > 0 && len + (usize)len2 < array_size(buf));
    return buf;
}

Ns operator-(const Time& lhs, const Time& rhs) noexcept
{
    auto a = lhs.stamp, b = rhs.stamp;
    fm_assert(a >= b);
    return Ns{a - b};
}

} // namespace floormat
