#include "timer.hpp"
#include "compat/assert.hpp"
#include <ctime>
#include <cstdio>
#include <chrono>
#include <mg/Time.h>

namespace floormat {

using std::chrono::duration_cast;
using std::chrono::duration;

using Clock = std::chrono::high_resolution_clock;
using SystemClock = std::chrono::system_clock;
using Nsecs = duration<uint64_t, std::nano>;
using Millis = duration<unsigned, std::milli>;

namespace {

template<typename T> struct array_size_;
template<typename T, size_t N> struct array_size_<T(&)[N]> : std::integral_constant<size_t, N> {};
template<typename T, size_t N> struct array_size_<std::array<T, N>> : std::integral_constant<size_t, N> {};
template<typename T> constexpr inline auto array_size = array_size_<T>::value;

uint64_t get_time() noexcept { return duration_cast<Nsecs>(Clock::now().time_since_epoch()).count(); }

const uint64_t Epoch = get_time();

} // namespace

Time Time::now() noexcept
{
    auto val = get_time();
    auto ret = val - Epoch;
    return {ret};
}

Ns operator-(const Time& a, const Time& b) noexcept
{
    fm_assert(a.stamp >= b.stamp);
    auto ret = a.stamp - b.stamp;
    fm_assert(ret < uint64_t{1} << 63);
    return Ns{ int64_t(ret) };
}

Ns Time::update(const Time& ts) & noexcept
{
    auto ret = ts - *this;
    stamp = ts.stamp;
    return ret;
}

uint64_t Time::init() noexcept { return get_time(); }
bool Time::operator==(const Time&) const noexcept = default;
std::strong_ordering Time::operator<=>(const Time&) const noexcept = default;

float Time::to_seconds(const Ns& ts) noexcept { return float(Ns::Type{ts} * 1e-9L); }
float Time::to_milliseconds(const Ns& ts) noexcept { return float(Ns::Type{ts} * 1e-6L); }

const char* format_datetime_to_string(char (&buf)[fm_DATETIME_BUF_SIZE])
{
    constexpr const char* fmt = "%a, %d %b %Y %H:%M:%S.";
    constexpr size_t fmtsize = std::size("Thu 01 Mon 197000 00:00:00.");
    static_assert(array_size<decltype(buf)> - fmtsize == 4);
    const auto t    = SystemClock::now();
    const auto ms   = duration_cast<Millis>(t.time_since_epoch()) % 1000;
    const auto time = SystemClock::to_time_t(t);
    const auto* tm  = std::localtime(&time);
    auto len = std::strftime(buf, std::size(buf), fmt, tm);
    fm_assert(len > 0 && len <= fmtsize);
    auto len2 = std::sprintf(buf + len, "%03u", unsigned{ms.count()});
    fm_assert(len2 > 0 && len + (size_t)len2 < std::size(buf));
    return buf;
}

} // namespace floormat
