#pragma once
#include "defs.hpp"
#include <cstdio>
#include <limits>
#include <type_traits>

namespace Magnum::Examples {

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

#define OUT_OF_RANGE(value, min, max)                                       \
    ::Magnum::Examples::out_of_range{                                       \
        {__FILE__, FUNCTION_NAME, __LINE__},                                \
        ::Magnum::Examples::ssize_t((value)),                               \
        ::Magnum::Examples::ssize_t((min)),                                 \
        ::Magnum::Examples::ssize_t((max))                                  \
    }

#define ABORT_WITH(exc_type, ...) ([&]() { \
    if (std::is_constant_evaluated()) {                                     \
        exc_type _e;                                                        \
        _e.line = __LINE__;                                                 \
        _e.file = __FILE__;                                                 \
        _e.function = FUNCTION_NAME;                                        \
        std::snprintf(_e.msg, sizeof(_e.msg), __VA_ARGS__);                 \
        throw _e;/*NOLINT(misc-throw-by-value-catch-by-reference)*/         \
    } else                                                                  \
        throw "aborting";                                                   \
}())

#define ABORT(...) \
    do {                                                                    \
        if (std::is_constant_evaluated())                                   \
            throw "aborting";                                               \
        else                                                                \
            ABORT_WITH(::Magnum::Examples::assertion_failure, __VA_ARGS__); \
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
