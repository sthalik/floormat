#pragma once
#include "compat/format.hpp"
#include <type_traits>
#include <iterator>
#include <exception>

namespace floormat {

struct base_exception {};

struct exception : std::exception, base_exception
{
    template<typename Fmt, typename... Ts>
    exception(const Fmt& fmt, Ts&&... args) noexcept;

    const char* what() const noexcept override;

private:
    fmt::memory_buffer buf;
};

template<typename Fmt, typename... Ts>
exception::exception(const Fmt& fmt, Ts&&... args) noexcept
{
    fmt::format_to(std::back_inserter(buf), fmt, Utility::forward<Ts>(args)...);
    buf.push_back('\0');
}

} // namespace floormat

#define fm_soft_assert(...)                                                         \
    do {                                                                            \
        if (!(__VA_ARGS__))                                                         \
        {                                                                           \
            if (std::is_constant_evaluated())                                       \
                throw ::floormat::base_exception{};                                 \
            else                                                                    \
                throw ::floormat::exception{"assertion failed: {} in {}:{}"_cf,     \
                                #__VA_ARGS__, __FILE__, __LINE__};                  \
        }                                                                           \
    } while (false)

#define fm_throw(fmt, ...)                                                          \
    do {                                                                            \
        if (std::is_constant_evaluated())                                           \
            throw ::floormat::base_exception{};                                     \
        else                                                                        \
            throw ::floormat::exception{fmt, __VA_ARGS__};                          \
    } while (false)
