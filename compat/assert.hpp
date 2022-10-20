#pragma once
#include "defs.hpp"
#include <cstdlib>
#include <cstdio>
#include <type_traits>

#ifdef __GNUG__
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-macros"
#endif

#define fm_EMIT_DEBUG(pfx, ...)                                         \
    do {                                                                \
        if (!std::is_constant_evaluated()) {                            \
            if constexpr (sizeof(pfx) > 1)                              \
                std::fputs((pfx), stderr);                              \
            std::fprintf(stderr, __VA_ARGS__);                          \
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

#ifdef __GNUG__
#   pragma GCC diagnostic pop
#endif

namespace floormat {

template<bool>
struct static_warning_ final {
    [[deprecated("compile-time warning valuated to 'true'")]] constexpr static_warning_() = default;
};

template<>
struct static_warning_<true> final {
    constexpr static_warning_() = default;
};

#define static_warning(...) do { (void)static_warning_<(__VA_ARGS__)>{};  } while(false)

} // namespace floormat

