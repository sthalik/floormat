#pragma once
#include <cstdio>
#include <exception>

namespace Magnum::Examples {

struct assertion_failure final : std::exception
{
    const char* file = nullptr;
    int line = -1;
    char msg[128 - sizeof(int) - sizeof(char*)];

    const char* what() const noexcept override { return msg; }
};

} // namespace Magnum::Examples

#define ABORT_WITH(exc_type, ...) ([&]() {                      \
    exc_type _e;                                                \
    _e.line = __LINE__;                                         \
    _e.file = __FILE__;                                         \
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

