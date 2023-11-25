#pragma once
#include "defs.hpp"
#include <cstdlib>
#include <cstdio>
#include <type_traits>

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-macros"
#ifdef __clang__
#   define FM_KILL_PRINTF_WARN_1_2() \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wformat-nonliteral\"")
#   define FM_KILL_PRINTF_WARN_2_2() _Pragma("clang diagnostic pop")
#else
#define FM_KILL_PRINTF_WARN_1_2()
#define FM_KILL_PRINTF_WARN_2_2()
#endif

#define FM_KILL_PRINTF_WARN_1()                              \
    _Pragma("GCC diagnostic push")                           \
    _Pragma("GCC diagnostic ignored \"-Wdouble-promotion\"") \
    FM_KILL_PRINTF_WARN_1_2()

#define FM_KILL_PRINTF_WARN_2() _Pragma("GCC diagnostic pop") FM_KILL_PRINTF_WARN_2_2()
#else
#define FM_KILL_PRINTF_WARN_1()
#define FM_KILL_PRINTF_WARN_2()
#endif

#define fm_EMIT_ABORT() ::std::abort();

#define fm_EMIT_DEBUG2(pfx, ...)                                        \
    do {                                                                \
        if (!std::is_constant_evaluated())                              \
        {                                                               \
            if constexpr (sizeof(pfx) > 1)                              \
                std::fputs((pfx), stderr);                              \
            FM_KILL_PRINTF_WARN_1()                                     \
            std::fprintf(stderr, __VA_ARGS__);                          \
            FM_KILL_PRINTF_WARN_2()                                     \
        }                                                               \
    } while (false)

#define fm_EMIT_DEBUG(pfx, ...)                                         \
    do {                                                                \
        if (!std::is_constant_evaluated())                              \
        {                                                               \
            fm_EMIT_DEBUG2(pfx, __VA_ARGS__);                           \
            std::fputc('\n', stderr);                                   \
            std::fflush(stderr);                                        \
        }                                                               \
    } while (false)

#define fm_abort(...)                                                   \
    do {                                                                \
        fm_EMIT_DEBUG2("fatal: ", __VA_ARGS__);                         \
        fm_EMIT_DEBUG("", " in %s:%d", __FILE__, __LINE__);             \
        fm_EMIT_ABORT();                                                \
    } while (false)

#define fm_assert(...)                                                  \
    do {                                                                \
        if (!(__VA_ARGS__)) [[unlikely]] {                              \
            fm_EMIT_DEBUG("", "assertion failed: %s in %s:%d",          \
                          #__VA_ARGS__, __FILE__, __LINE__);            \
            fm_EMIT_ABORT();                                            \
        }                                                               \
    } while(false)

#ifndef FM_NO_DEBUG
#define fm_debug_assert(...) fm_assert(__VA_ARGS__)
#else
#define fm_debug_assert(...) void()
#endif

#define fm_warn(...)  fm_EMIT_DEBUG("warning: ", __VA_ARGS__)
#define fm_error(...) fm_EMIT_DEBUG("error: ", __VA_ARGS__)
#define fm_log(...)   fm_EMIT_DEBUG("", __VA_ARGS__)
#define fm_debug(...) fm_EMIT_DEBUG("", __VA_ARGS__)

#define fm_warn_once(...) do {                                          \
        static bool _fm_once_flag = false;                              \
        if (!_fm_once_flag) [[unlikely]] {                              \
            _fm_once_flag = true;                                       \
            fm_warn(__VA_ARGS__);                                       \
        }                                                               \
    } while (false)

#define fm_assert_equal(...)                                            \
    ([](auto a, auto b) -> void                                         \
    {                                                                   \
        if (a != b) [[unlikely]]                                        \
        {                                                               \
            ERR_nospace << Debug::color(Debug::Color::Magenta)          \
                        << "fatal:"                                     \
                        << Debug::resetColor << " "                     \
                        << "Equality assertion failed at "              \
                        << __FILE__ << ":" << __LINE__;                 \
            ERR_nospace << #__VA_ARGS__;                                \
            ERR_nospace << "    expected: " << a;                       \
            ERR_nospace << "      actual: " << b;                       \
            fm_EMIT_ABORT();                                            \
        }                                                               \
    })(__VA_ARGS__)

#ifdef __GNUG__
#   pragma GCC diagnostic pop
#endif
