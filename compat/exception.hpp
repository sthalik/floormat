#pragma once
#include <fmt/base.h>
#include <cr/String.h>
#include <exception>

namespace floormat {

struct base_exception {};

namespace detail {

struct exception_tag {};

template<typename T> struct exception_arg { using type = T; };
template<> struct exception_arg<Corrade::Containers::StringView> { using type = fmt::string_view; };
template<> struct exception_arg<Corrade::Containers::String> { using type = fmt::string_view; };
template<typename T> using exception_arg_t = typename exception_arg<std::decay_t<T>>::type;

template<typename T> const T& exception_conv(const T& x) { return x; }
inline fmt::string_view exception_conv(const Corrade::Containers::StringView& s) { return {s.data(), s.size()}; }
inline fmt::string_view exception_conv(const Corrade::Containers::String& s) { return {s.data(), s.size()}; }

} // namespace detail

struct exception : std::exception, base_exception
{
    template<typename... Ts>
    exception(fmt::format_string<detail::exception_arg_t<Ts>...> str, const Ts&... args) noexcept;

    exception(exception&&) noexcept;
    exception(const exception& other) noexcept;
    exception& operator=(exception&&) noexcept;
    exception& operator=(const exception& other) noexcept;
    const char* what() const noexcept override;

private:
    exception(detail::exception_tag, fmt::string_view str, fmt::format_args args) noexcept;

    Corrade::Containers::String buf;
};

template<typename... Ts>
exception::exception(fmt::format_string<detail::exception_arg_t<Ts>...> str, const Ts&... args) noexcept :
    exception{detail::exception_tag{}, str.get(),
              fmt::vargs<detail::exception_arg_t<Ts>...>{{detail::exception_conv(args)...}}}
{
}

} // namespace floormat

// The args go through `const Ts&...`, where a string literal deduces char[N]. Passing
// them raw made every distinct expression length its own ctor specialization.
#define fm_soft_assert(...)                                                         \
    do {                                                                            \
        if (!(__VA_ARGS__)) /*NOLINT(*-simplify-boolean-expr)*/                     \
        {                                                                           \
            if (std::is_constant_evaluated())                                       \
                throw ::floormat::base_exception{};                                 \
            else                                                                    \
                throw ::floormat::exception{                                        \
                    "assertion failed: {} in {}:{}",                                \
                    ::Corrade::Containers::StringView{#__VA_ARGS__},                \
                    ::Corrade::Containers::StringView{__FILE__},                    \
                    (::floormat::uint32_t)__LINE__                                  \
                };                                                                  \
        }                                                                           \
    } while (false)

#define fm_throw(fmt, ...)                                                          \
    do {                                                                            \
        if (std::is_constant_evaluated())                                           \
            throw ::floormat::base_exception{};                                     \
        else                                                                        \
            throw ::floormat::exception{fmt, __VA_ARGS__};                          \
    } while (false)
