#pragma once
#include "compat/format.hpp"

namespace floormat { struct base_exception {}; }

namespace floormat::detail {

[[noreturn]] void vthrow_fmt(fmt::string_view str, fmt::format_args args) noexcept(false);
[[noreturn]] void throw_soft_assert(const char* expr, const char* file, int line) noexcept(false);

template<typename... Ts>
[[noreturn]] void throw_fmt(fmt::format_string<Ts...> str, Ts&&... args) noexcept(false)
{
    vthrow_fmt(str.get(), fmt::vargs<Ts...>{{args...}});
}

} // namespace floormat::detail

#define fm_soft_assert(...)                                                         \
    do {                                                                            \
        if (!(__VA_ARGS__)) /*NOLINT(*-simplify-boolean-expr)*/                     \
        {                                                                           \
            if (std::is_constant_evaluated())                                       \
                throw ::floormat::base_exception{};                                 \
            else                                                                    \
                ::floormat::detail::throw_soft_assert(#__VA_ARGS__, __FILE__,       \
                                                      __LINE__);                    \
        }                                                                           \
    } while (false)

#define fm_throw(...)                                                               \
    do {                                                                            \
        if (std::is_constant_evaluated())                                           \
            throw ::floormat::base_exception{};                                     \
        else                                                                        \
            ::floormat::detail::throw_fmt(__VA_ARGS__);                             \
    } while (false)
