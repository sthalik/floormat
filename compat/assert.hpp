#pragma once
#include "defs.hpp"
#include <cstdlib>
#include <cstdio>
#include <type_traits>

namespace Magnum::Examples::detail {

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
    if (std::is_constant_evaluated())
        throw "aborting";
    else {
        emit_debug("fatal: ", fmt, xs...);
        std::abort();
    }
}

} // namespace Magnum::Examples::detail

#define ABORT(...) ::Magnum::Examples::detail::abort(__VA_ARGS__)

#define ASSERT(expr)                                                \
    do {                                                            \
        if (!(expr)) {                                              \
            ::Magnum::Examples::detail::                            \
                abort("assertion failed: '%s' in %s:%d",            \
                      #expr, __FILE__, __LINE__);                   \
        }                                                           \
    } while(false)

#define ASSERT_EXPR(var, expr, cond)                                \
    ([&] {                                                          \
        decltype(auto) var = (expr);                                \
        ASSERT(cond);                                               \
        return (var);                                               \
    })()

#define WARN(...)       ::Magnum::Examples::detail::emit_debug("warning: ", __VA_ARGS__)
#define ERR(...)        ::Magnum::Examples::detail::emit_debug("error: ", __VA_ARGS__)
#define MESSAGE(...)    ::Magnum::Examples::detail::emit_debug("", __VA_ARGS__)
#define DEBUG(...)      ::Magnum::Examples::detail::emit_debug("", __VA_ARGS__)
