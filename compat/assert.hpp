#pragma once
#include <cstdlib>
#include <cstdio>
#include <type_traits>

namespace Magnum::Examples::detail {

template<typename...Xs>
constexpr inline void abort(const char* fmt, Xs... xs)
{
    if (std::is_constant_evaluated()) {
        std::fprintf(stderr, fmt, xs...);
        std::putc('\n', stderr);
        std::fflush(stderr);
        std::abort();
    } else
        throw "aborting";
}

} // namespace Magnum::Examples::detail

namespace Magnum::Examples {

#define ABORT(...) \
    do {                                                                    \
        if (std::is_constant_evaluated())                                   \
            throw "aborting";                                               \
        else                                                                \
            ::Magnum::Examples::detail::                                    \
                abort("aborting at %s:%d", __FILE__, __LINE__);             \
    } while (false)

#define ASSERT(expr)                                                        \
    do {                                                                    \
        if (!(expr)) {                                                      \
            ABORT("assertion failed: '%s' in %s:%d",                        \
                  #expr, __FILE__, __LINE__);                               \
        }                                                                   \
    } while(false)

#define GAME_DEBUG_OUT(pfx, ...) ([&]() {                                   \
    if constexpr (sizeof((pfx)) > 1)                                        \
        std::fputs((pfx), stderr);                                          \
    std::fprintf(stderr, __VA_ARGS__);                                      \
    std::fputs("\n", stderr);                                               \
    std::fflush(stderr);                                                    \
}())

#define WARN(...)   GAME_DEBUG_OUT("warning: ", __VA_ARGS__)
#define ERR(...)    GAME_DEBUG_OUT("error: ", __VA_ARGS__)
#define DEBUG(...)  GAME_DEBUG_OUT("", __VA_ARGS__)

} // namespace Magnum::Examples
