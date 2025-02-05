#include "assert.hpp"
#include "exception.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif

namespace floormat::debug::detail {

namespace {

bool do_soft_assert = false;

template<bool DoPrefix, bool DoSourceLocation>
CORRADE_NEVER_INLINE
void emit_debug_(const char* prefix, const char* file, int line, const char* fmt, va_list arg_ptr)
{
    std::fflush(stdout);
    std::fflush(stderr);
    if constexpr(DoPrefix)
        std::fputs(prefix, stderr);
    std::vfprintf(stderr, fmt, arg_ptr);
    if constexpr(DoSourceLocation)
        std::fprintf(stderr, " in %s:%d\n", file, line);
    else
        std::fputc('\n', stderr);
    std::fflush(stderr);
}

} // namespace

void emit_debug(const char* prefix, fm_FORMAT_ARG_MSVC const char* fmt, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    emit_debug_<true, false>(prefix, nullptr, 0, fmt, arg_ptr);
    va_end(arg_ptr);
}

void emit_debug0(fm_FORMAT_ARG_MSVC const char* fmt, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    emit_debug_<false, false>(nullptr, nullptr, 0, fmt, arg_ptr);
    va_end(arg_ptr);
}

void CORRADE_NEVER_INLINE emit_debug_loc(const char* prefix, const char* file, int line, fm_FORMAT_ARG_MSVC const char* fmt, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    emit_debug_<true, true>(prefix, file, line, fmt, arg_ptr);
    va_end(arg_ptr);
}

void emit_assert_fail(const char* expr, const char* file, int line)
{
    std::fflush(stdout);
    std::fflush(stderr);
    std::fprintf(stderr, "assertion failed: %s in %s:%d\n", expr, file, line);
    std::fflush(stderr);
    std::abort();
}

void emit_abort(const char* file, int line, fm_FORMAT_ARG_MSVC const char* fmt, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    emit_debug_<true, true>("fatal: ", file, line, fmt, arg_ptr);
    va_end(arg_ptr);
    std::abort();
}

void emit_abort()
{
    std::fflush(stdout);
    std::fflush(stderr);
    std::abort();
}

} // namespace floormat::debug::detail

using namespace floormat::debug::detail;

namespace floormat::debug {

void set_soft_assert_mode(bool value) { do_soft_assert = value; }
bool soft_assert_mode() { return detail::do_soft_assert; }

} // namespace floormat::debug
