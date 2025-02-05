#pragma once
#include "compat/assert.hpp"
#include <fmt/core.h>
#include <fmt/compile.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/String.h>

namespace fmt {

template<> struct formatter<Corrade::Containers::StringView> {
  template<typename ParseContext> static constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
  template<typename FormatContext> auto format(Corrade::Containers::StringView const& s, FormatContext& ctx) const;
};

template<> struct formatter<Corrade::Containers::String> {
  template<typename ParseContext> static constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
  template<typename FormatContext> auto format(Corrade::Containers::String const& s, FormatContext& ctx) const;
};

} // namespace fmt

#if !FMT_USE_NONTYPE_TEMPLATE_ARGS
namespace floormat::detail::fmt {
template<size_t N>
struct fmt_string final {
    static constexpr size_t size = N;
    char data[N];

    template <size_t... Is>
    consteval fmt_string(const char (&arr)[N]) noexcept {
        for (auto i = 0uz; i < N; i++)
            data[i] = arr[i];
    }
};
} // namespace floormat::detail::fmt
#endif

#if !FMT_USE_NONTYPE_TEMPLATE_ARGS
template<::floormat::detail::fmt::fmt_string s>
consteval auto operator""_cf() noexcept
{
    return FMT_COMPILE(s.data);
}
#else
using fmt::literals::operator""_cf;
#endif

namespace floormat {

template<size_t N, typename Fmt, typename... Xs>
size_t snformat(char(&buf)[N], Fmt&& fmt, Xs&&... args)
{
    constexpr size_t n = N > 0 ? N - 1 : 0;
    auto result = fmt::format_to_n(buf, n, forward<Fmt>(fmt), forward<Xs>(args)...);
    const auto len = n < result.size ? n : result.size;
    fm_assert(len > 0);
#if 0
    if constexpr(N > 0)
        buf[len] = '\0';
#else
    buf[len] = '\0';
#endif
    return result.size;
}

} // namespace floormat

template<typename FormatContext>
auto fmt::formatter<Corrade::Containers::StringView>::format(Corrade::Containers::StringView const& s, FormatContext& ctx) const {
  return fmt::format_to(ctx.out(), "{}"_cf, basic_string_view<char>{s.data(), s.size()});
}

template<typename FormatContext>
auto fmt::formatter<Corrade::Containers::String>::format(Corrade::Containers::String const& s, FormatContext& ctx) const {
  return fmt::format_to(ctx.out(), "{}"_cf, basic_string_view<char>{s.data(), s.size()});
}
