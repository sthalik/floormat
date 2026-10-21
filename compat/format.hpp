#pragma once
#include <fmt/core.h>
#include <cr/String.h>

namespace fmt {

// Inherit parse() from the string formatter, or every format spec is a parse error:
// a parse() that consumes nothing leaves the reader on '<' of "{:<44}" instead of '}'.
template<> struct formatter<Corrade::Containers::StringView> : formatter<basic_string_view<char>> {
  auto format(Corrade::Containers::StringView const& s, format_context& ctx) const -> format_context::iterator;
};

template<> struct formatter<Corrade::Containers::String> : formatter<basic_string_view<char>> {
  auto format(Corrade::Containers::String const& s, format_context& ctx) const -> format_context::iterator;
};

} // namespace fmt

namespace floormat {

template<size_t N, typename... Xs>
size_t snformat(char(&buf)[N], fmt::format_string<Xs...> fmt, Xs&&... args)
{
    constexpr size_t n = N > 0 ? N - 1 : 0;
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
