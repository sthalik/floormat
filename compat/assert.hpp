#pragma once
//#include "defs.hpp"
#if defined __GNUC__ || defined __clang__
    // If we're on MinGW, we usually want gnu_printf to support C99 features
    #if defined(__MINGW32__)
        #define fm_FORMAT_ARG(n) __attribute__((format(gnu_printf, (n), ((n)+1))))
    #else
        // macOS, Linux Clang, and modern GCC are happy with plain 'printf'
        #define fm_FORMAT_ARG(n) __attribute__((format(printf, (n), ((n)+1))))
    #endif
    #define fm_FORMAT_ARG_MSVC
#elif defined _MSC_VER
    #include <sal.h>
    #define fm_FORMAT_ARG(n)
    #define fm_FORMAT_ARG_MSVC _Printf_format_string_
#else
    #define fm_FORMAT_ARG(n)
    #define fm_FORMAT_ARG_MSVC
#endif

namespace floormat::debug::detail {

void emit_debug(const char* prefix, fm_FORMAT_ARG_MSVC const char* fmt, ...) fm_FORMAT_ARG(2);
void emit_debug0(fm_FORMAT_ARG_MSVC const char* fmt, ...) fm_FORMAT_ARG(1);
void emit_debug_loc(const char* prefix, const char* file, int line, fm_FORMAT_ARG_MSVC const char* fmt, ...) fm_FORMAT_ARG(4);
void emit_debug_loc0(const char* file, int line, fm_FORMAT_ARG_MSVC const char* fmt, ...) fm_FORMAT_ARG(3);

[[noreturn]] CORRADE_NEVER_INLINE void emit_assert_fail(const char* expr, const char* file, int line);
[[noreturn]] CORRADE_NEVER_INLINE void emit_abort(const char* file, int line, fm_FORMAT_ARG_MSVC const char* fmt, ...) fm_FORMAT_ARG(3);
[[noreturn]] CORRADE_NEVER_INLINE void emit_abort();

} // namespace floormat::debug::detail


namespace floormat::debug {

void set_soft_assert_mode(bool value);
bool soft_assert_mode();

} // namespace floormat::debug


namespace floormat {

} // namespace floormat

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-macros"
#endif

#define fm_assert(...) ((__VA_ARGS__) ? void() : ::floormat::debug::detail::emit_assert_fail(#__VA_ARGS__, __FILE__, __LINE__))
#define fm_abort(...)           (::floormat::debug::detail::emit_abort(__FILE__, __LINE__, __VA_ARGS__))
#define fm_warn(...)            (::floormat::debug::detail::emit_debug("warning: ", __VA_ARGS__))
#define fm_error(...)           (::floormat::debug::detail::emit_debug("error: ", __VA_ARGS__))
#define fm_log(...)             (::floormat::debug::detail::emit_debug0(__VA_ARGS__))
#define fm_debug(...)           (::floormat::debug::detail::emit_debug0(__VA_ARGS__))
#define fm_debug_loc(pfx, ...)  (::floormat::debug::detail::emit_debug_loc(pfx, __FILE__, __LINE__,__VA_ARGS__))
#define fm_debug_loc0(...)      (::floormat::debug::detail::emit_debug_loc0(__FILE__, __LINE__,__VA_ARGS__))

#if defined FM_NO_DEBUG && !defined FM_NO_DEBUG2
#define FM_NO_DEBUG2
#endif

#ifndef FM_NO_DEBUG
#define fm_debug_assert(...) fm_assert(__VA_ARGS__)
#else
#define fm_debug_assert(...) (void())
#endif

#ifndef FM_NO_DEBUG2
#define fm_debug2_assert(...) fm_assert(__VA_ARGS__)
#else
#define fm_debug2_assert(...) (void())
#endif

#define fm_warn_once(...) do {                                          \
        static bool _fm_once_flag = false;                              \
        if (!_fm_once_flag) [[unlikely]] {                              \
            _fm_once_flag = true;                                       \
            fm_warn(__VA_ARGS__);                                       \
        }                                                               \
    } while (false)

#define fm_assert_equal(...)                                            \
    ([](auto expected, auto actual) -> void                             \
    {                                                                   \
        if (expected != actual) [[unlikely]]                            \
        {                                                               \
            ERR_nospace << Debug::color(Debug::Color::Magenta)          \
                        << "fatal:"                                     \
                        << Debug::resetColor << " "                     \
                        << "Equality assertion failed at "              \
                        << __FILE__ << ":" << __LINE__;                 \
            ERR_nospace << "fm_assert_equal(" << #__VA_ARGS__ << ")";   \
            ERR_nospace << "    expected: " << expected;                \
            ERR_nospace << "      actual: " << actual;                  \
            ::floormat::debug::detail::emit_abort();                    \
        }                                                               \
    })(__VA_ARGS__)

#define fm_assert_not_equal(...) \
    ([](auto not_expected, auto actual) -> void                         \
    {                                                                   \
        if (not_expected == actual) [[unlikely]]                        \
        {                                                               \
            ERR_nospace << Debug::color(Debug::Color::Magenta)          \
                        << "fatal:"                                     \
                        << Debug::resetColor << " "                     \
                        << "Inequality assertion failed at "            \
                        << __FILE__ << ":" << __LINE__;                 \
            ERR_nospace << "fm_assert_not_equal(" << #__VA_ARGS__ << ")"; \
            ERR_nospace << "not expected: " << not_expected;            \
            ERR_nospace << "      actual: " << actual;                  \
            ::floormat::debug::detail::emit_abort();                    \
        }                                                               \
        })(__VA_ARGS__)

#ifdef __GNUG__
#   pragma GCC diagnostic pop
#endif
