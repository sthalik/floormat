#pragma once
#include "defs.hpp"
#include <cstdlib>
#include <cstdio>
#include <type_traits>

namespace floormat::detail {

template<std::size_t N, std::size_t M, typename... Xs>
constexpr void emit_debug(const char(&pfx)[M], const char(&fmt)[N], Xs... xs) noexcept
{
    if (std::is_constant_evaluated())
        return;
    else {
        if constexpr (M > 1)
            std::fputs(pfx, stderr);
        std::fprintf(stderr, fmt, xs...);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }
}

template<std::size_t N, typename...Xs>
[[noreturn]]
constexpr inline void abort(const char (&fmt)[N], Xs... xs) noexcept
{
#if 0
    if (std::is_constant_evaluated())
        throw "aborting";
    else
#endif
    {
        emit_debug("fatal: ", fmt, xs...);
        std::abort();
    }
}

} // namespace floormat::detail

#define ABORT(...) ::floormat::detail::abort(__VA_ARGS__)

#define ASSERT(...)                                                 \
    do {                                                            \
        if (!(__VA_ARGS__)) {                                       \
            ::floormat::detail::                            \
                abort("assertion failed: '%s' in %s:%d",            \
                      #__VA_ARGS__, __FILE__, __LINE__);            \
        }                                                           \
    } while(false)

#define ASSERT_EXPR(var, expr, cond)                                \
    ([&] {                                                          \
        decltype(auto) var = (expr);                                \
        ASSERT(cond);                                               \
        return (var);                                               \
    })()

#define WARN(...)       ::floormat::detail::emit_debug("warning: ", __VA_ARGS__)
#define ERR(...)        ::floormat::detail::emit_debug("error: ", __VA_ARGS__)
#define MESSAGE(...)    ::floormat::detail::emit_debug("", __VA_ARGS__)
#define DEBUG(...)      ::floormat::detail::emit_debug("", __VA_ARGS__)

namespace floormat {

template<bool>
struct static_warning_ final {
    [[deprecated]] constexpr static_warning_() = default;
};

template<>
struct static_warning_<true> final {
    constexpr static_warning_() = default;
};

#define static_warning(...) do { (void)static_warning_<(__VA_ARGS__)>{};  } while(false)

} // namespace floormat
