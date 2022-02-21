#pragma once
#include <cstddef>
#include <cstdio>
#include <limits>
#include <type_traits>

#ifdef _MSC_VER
#   define FUNCTION_NAME __FUNCSIG__
#else
#   define FUNCTION_NAME __PRETTY_FUNCTION__
#endif

namespace Magnum::Examples {

using size_t = std::size_t;
using ssize_t = std::make_signed_t<std::size_t>;

struct exception {
    const char* file = nullptr;
    const char* function = nullptr;
    int line = -1;
};

struct assertion_failure final : exception
{
    char msg[128 - sizeof(int) - sizeof(char*)];
};

struct out_of_range final : exception {
    ssize_t value = 0;
    ssize_t min = std::numeric_limits<ssize_t>::min();
    ssize_t max = std::numeric_limits<ssize_t>::max();
};

struct key_error final : exception {
    ssize_t value = 0;
};

} // namespace Magnum::Examples

#define KEY_ERROR(value) \
    ::Magnum::Examples::key_error{{__FILE__, FUNCTION_NAME, __LINE__}, (value)}

#define OUT_OF_RANGE(value, min, max)                           \
    ::Magnum::Examples::out_of_range{                           \
        {__FILE__, FUNCTION_NAME, __LINE__},                    \
        ::Magnum::Examples::ssize_t((value)),                   \
        ::Magnum::Examples::ssize_t((min)),                     \
        ::Magnum::Examples::ssize_t((max))                      \
    }

#define ABORT_WITH(exc_type, ...) ([&]() {                      \
    exc_type _e;                                                \
    _e.line = __LINE__;                                         \
    _e.file = __FILE__;                                         \
    _e.function = FUNCTION_NAME;                                \
    std::snprintf(_e.msg, sizeof(_e.msg), __VA_ARGS__);         \
    throw _e;                                                   \
}())

#define ABORT(...) \
    ABORT_WITH(::Magnum::Examples::assertion_failure, __VA_ARGS__)

#define ASSERT(expr)                                            \
    do {                                                        \
        if (!(expr))                                            \
            ABORT("assertion failed: '%s' in %s:%d",            \
                  #expr, __FILE__, __LINE__);                   \
    } while(false)

#define GAME_DEBUG_OUT(pfx, ...) ([&]() {                       \
    if constexpr (sizeof((pfx)) > 1)                            \
        std::fputs((pfx), stderr);                              \
    std::fprintf(stderr, __VA_ARGS__);                          \
    std::fputs("\n", stderr);                                   \
    std::fflush(stderr);                                        \
}())

#define WARN(...)   GAME_DEBUG_OUT("warning: ", __VA_ARGS__)
#define ERR(...)    GAME_DEBUG_OUT("error: ", __VA_ARGS__)
#define DEBUG(...)  GAME_DEBUG_OUT("", __VA_ARGS__)

