#pragma once
#include "defs.hpp"
#include <cstdlib>
#include <cstdio>
#include <type_traits>

#ifdef __GNUG__
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-macros"
#define FM_KILL_WARN_double_promotion1()                                \
    _Pragma( "GCC diagnostic push" )                                    \
    _Pragma( "GCC diagnostic ignored \"-Wdouble-promotion\"" )
#define FM_KILL_WARN_double_promotion2() _Pragma( "GCC diagnostic pop" )
#else
#define FM_KILL_WARN_double_promotion1()
#define FM_KILL_WARN_double_promotion2()
#endif

#define fm_EMIT_DEBUG(pfx, ...)                                         \
    do {                                                                \
        if (!std::is_constant_evaluated()) {                            \
            if constexpr (sizeof(pfx) > 1)                              \
                std::fputs((pfx), stderr);                              \
            FM_KILL_WARN_double_promotion1()                            \
            std::fprintf(stderr, __VA_ARGS__);                          \
            FM_KILL_WARN_double_promotion2()                            \
            std::fputc('\n', stderr);                                   \
            std::fflush(stderr);                                        \
        }                                                               \
    } while (false)

#define fm_abort(...)                                                   \
    do {                                                                \
        fm_EMIT_DEBUG("fatal: ", __VA_ARGS__);                          \
        std::abort();                                                   \
    } while (false)

#define fm_assert(...)                                                  \
    do {                                                                \
        if (!(__VA_ARGS__)) {                                           \
            fm_EMIT_DEBUG("", "assertion failed: '%s' in %s:%d",        \
                       #__VA_ARGS__, __FILE__, __LINE__);               \
            std::abort();                                               \
        }                                                               \
    } while(false)

#define ASSERT_EXPR(var, expr, cond)                                    \
    ([&] {                                                              \
        decltype(auto) var = (expr);                                    \
            fm_assert(cond);                                            \
        return (var);                                                   \
    })()

#define fm_warn(...)  fm_EMIT_DEBUG("warning: ", __VA_ARGS__)
#define fm_error(...) fm_EMIT_DEBUG("error: ", __VA_ARGS__)
#define fm_log(...)   fm_EMIT_DEBUG("", __VA_ARGS__)
#define fm_debug(...) fm_EMIT_DEBUG("", __VA_ARGS__)

#define fm_warn_once(...) do {                                          \
        static bool _fm_once_flag = false;                              \
        if (!_fm_once_flag) {                                           \
            _fm_once_flag = true;                                       \
            fm_warn(__VA_ARGS__);                                       \
        }                                                               \
    } while (false)

#ifdef __GNUG__
#   pragma GCC diagnostic pop
#endif
