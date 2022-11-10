#pragma once
#include <fmt/core.h>
#include <fmt/compile.h>

#ifndef _MSC_VER
namespace floormat::detail::fmt {
template<std::size_t N>
struct fmt_string final {
    static constexpr std::size_t size = N;
    char data[N];

    template <std::size_t... Is>
    consteval fmt_string(const char (&arr)[N]) noexcept {
        for (std::size_t i = 0; i < N; i++)
            data[i] = arr[i];
    }
};
} // namespace floormat::detail::fmt
#endif

namespace floormat {

#ifndef _MSC_VER
template<::floormat::detail::fmt::fmt_string s>
consteval auto operator""_cf() noexcept
{
    return FMT_COMPILE(s.data);
}
#else
using namespace fmt::literals;
#endif

template<std::size_t N, typename Fmt, typename... Xs>
std::size_t snformat(char(&buf)[N], Fmt&& fmt, Xs&&... args)
{
    constexpr std::size_t n = N > 0 ? N - 1 : 0;
    auto result = fmt::format_to_n(buf, n, std::forward<Fmt>(fmt), std::forward<Xs>(args)...);
    const auto len = std::min(n, result.size);
    if constexpr(N > 0)
        buf[len] = '\0';
    return len;
}

} // namespace floormat
