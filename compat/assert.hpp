#pragma once
#include "defs.hpp"
#include <cstdlib>
#include <cstdio>
#include <type_traits>

#ifdef __GNUG__
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-macros"
#endif

#define emit_debug(pfx, ...)                    \
    do {                                        \
        if (!std::is_constant_evaluated()) {    \
            if constexpr (sizeof(pfx) > 1)      \
                std::fputs((pfx), stderr);      \
            std::fprintf(stderr, __VA_ARGS__);  \
            std::fputc('\n', stderr);           \
            std::fflush(stderr);                \
        }                                       \
    } while (false)

#define ABORT(...) do { emit_debug("fatal: ", __VA_ARGS__); std::abort(); } while (false)

#define ASSERT(...)                                                     \
    do {                                                                \
        if (!(__VA_ARGS__)) {                                           \
            emit_debug("", "assertion failed: '%s' in %s:%d",           \
                       #__VA_ARGS__, __FILE__, __LINE__);               \
            std::abort();                                               \
        }                                                               \
    } while(false)

#define ASSERT_EXPR(var, expr, cond)                                    \
    ([&] {                                                              \
        decltype(auto) var = (expr);                                    \
        ASSERT(cond);                                                   \
        return (var);                                                   \
    })()

#define WARN(...)       emit_debug("warning: ", __VA_ARGS__)
#define ERR(...)        emit_debug("error: ", __VA_ARGS__)
#define MESSAGE(...)    emit_debug("", __VA_ARGS__)
#define DEBUG(...)      emit_debug("", __VA_ARGS__)

namespace floormat {

template<bool>
struct static_warning_ final {
    [[deprecated]] constexpr static_warning_() = default;
};

template<>
struct static_warning_<true> final {
    constexpr static_warning_() = default;
};

#define static_warning(...) do { (void)static_warning_<(__VA_ARGS__)>{};  } while(false)

} // namespace floormat

#ifdef __GNUG__
#   pragma GCC diagnostic pop
#endif
