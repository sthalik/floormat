#pragma once
#include <fmt/core.h>
#include <fmt/compile.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/String.h>

namespace fmt {

template<> struct formatter<Corrade::Containers::StringView> {
  template<typename ParseContext> static constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
  template<typename FormatContext> auto format(Corrade::Containers::StringView const& s, FormatContext& ctx);
};

template<> struct formatter<Corrade::Containers::String> {
  template<typename ParseContext> static constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }
  template<typename FormatContext> auto format(Corrade::Containers::String const& s, FormatContext& ctx);
};

} // namespace fmt

#if !FMT_USE_NONTYPE_TEMPLATE_ARGS
namespace floormat::detail::fmt {
template<std::size_t N>
struct fmt_string final {
    static constexpr std::size_t size = N;
    char data[N];

    template <std::size_t... Is>
    consteval fmt_string(const char (&arr)[N]) noexcept {
        for (auto i = 0_uz; i < N; i++)
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
using namespace fmt::literals;
#endif

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

template<typename FormatContext>
auto fmt::formatter<Corrade::Containers::StringView>::format(Corrade::Containers::StringView const& s, FormatContext& ctx) {
  return fmt::format_to(ctx.out(), "{}"_cf, basic_string_view<char>{s.data(), s.size()});
}

template<typename FormatContext>
auto fmt::formatter<Corrade::Containers::String>::format(Corrade::Containers::String const& s, FormatContext& ctx) {
  return fmt::format_to(ctx.out(), "{}"_cf, basic_string_view<char>{s.data(), s.size()});
}
