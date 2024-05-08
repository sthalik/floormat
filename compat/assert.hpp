#pragma once
//#include "defs.hpp"
#if defined __GNUG__ || defined __CLION_IDE__
#define fm_FORMAT_ARG(n) __attribute__((format (gnu_printf, (n), ((n)+1))))
#define fm_FORMAT_ARG_MSVC
#elif defined _MSC_VER
#include <sal.h>
#define fm_FORMAT_ARG(n)
#define fm_FORMAT_ARG_MSVC _Printf_format_string_
#else
#define fm_FORMAT_ARG(n)
#define fm_FORMAT_ARG_MSVC
#endif

namespace floormat {

void fm_emit_debug(const char* prefix, fm_FORMAT_ARG_MSVC const char* fmt, ...) fm_FORMAT_ARG(2);
void fm_emit_debug0(fm_FORMAT_ARG_MSVC const char* fmt, ...) fm_FORMAT_ARG(1);
void fm_emit_debug_loc(const char* prefix, const char* file, int line, fm_FORMAT_ARG_MSVC const char* fmt, ...) fm_FORMAT_ARG(4);
void fm_emit_debug_loc0(const char* file, int line, fm_FORMAT_ARG_MSVC const char* fmt, ...) fm_FORMAT_ARG(3);

[[noreturn]] CORRADE_NEVER_INLINE void fm_emit_assert_fail(const char* expr, const char* file, int line);
[[noreturn]] CORRADE_NEVER_INLINE void fm_emit_abort(const char* file, int line, fm_FORMAT_ARG_MSVC const char* fmt, ...) fm_FORMAT_ARG(3);
[[noreturn]] CORRADE_NEVER_INLINE void fm_emit_abort();

} // namespace floormat

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-macros"
#endif

#define fm_assert(...) ((__VA_ARGS__) ? void() : ::floormat::fm_emit_assert_fail(#__VA_ARGS__, __FILE__, __LINE__))
#define fm_abort(...)           (::floormat::fm_emit_abort(__FILE__, __LINE__, __VA_ARGS__))
#define fm_warn(...)            (::floormat::fm_emit_debug("warning: ", __VA_ARGS__))
#define fm_error(...)           (::floormat::fm_emit_debug("error: ", __VA_ARGS__))
#define fm_log(...)             (::floormat::fm_emit_debug0(__VA_ARGS__))
#define fm_debug(...)           (::floormat::fm_emit_debug0(__VA_ARGS__))
#define fm_debug_loc(pfx, ...)  (::floormat::fm_emit_debug_loc(pfx, __FILE__, __LINE__,__VA_ARGS__))
#define fm_debug_loc0(...)      (::floormat::fm_emit_debug_loc0(__FILE__, __LINE__,__VA_ARGS__))

#ifndef FM_NO_DEBUG
#define fm_debug_assert(...) fm_assert(__VA_ARGS__)
#else
#define fm_debug_assert(...) void()
#endif

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
            fm_emit_abort();                                            \
        }                                                               \
    })(__VA_ARGS__)

#ifdef __GNUG__
#   pragma GCC diagnostic pop
#endif
