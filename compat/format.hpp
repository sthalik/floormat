#pragma once
#include <fmt/core.h>
#include <fmt/compile.h>

namespace floormat {

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
