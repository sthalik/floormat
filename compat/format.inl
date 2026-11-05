#pragma once
#include "compat/format.hpp"

namespace floormat {

template<usize N, typename... Xs>
usize snformat(char(&buf)[N], fmt::format_string<Xs...> fmt, Xs&&... args)
{
    constexpr usize n = N > 0 ? N - 1 : 0;
    auto result = fmt::format_to_n(buf, n, fmt, floormat::forward<Xs>(args)...);
    const auto len = n < result.size ? n : result.size;
#if 0
    if constexpr(N > 0)
        buf[len] = '\0';
#else
    buf[len] = '\0';
#endif
    return result.size;
}

} // namespace floormat
