#pragma once
#include "function-view.hpp"
#include <source_location>
#include <type_traits>
#include <cr/Optional.h>
#include <cr/Pair.h>
#include <cr/Reference.h>
#include <cr/ScopeGuard.h>
#include <cr/StringView.h>

namespace floormat {

enum class gl_error_policy : u8 { warn, error, ignore, };

namespace detail {

[[nodiscard]] bool check_glcall(function_view<void()> fn, StringView expression, gl_error_policy policy,
                                std::source_location location = std::source_location::current());

} // namespace detail

struct with_gl_errors final
{
    explicit with_gl_errors(gl_error_policy policy = gl_error_policy::warn,
                            std::source_location location = std::source_location::current());
    with_gl_errors(gl_error_policy policy, StringView expression,
                   std::source_location location = std::source_location::current());
    ~with_gl_errors() noexcept(false);

private:
    friend bool detail::check_glcall(function_view<void()> fn, StringView expression, gl_error_policy policy,
                                     std::source_location location);

    bool close();

    StringView expression;
    std::source_location location;
    with_gl_errors* enclosing_scope;
    gl_error_policy policy;
    u8 error_mask = 0;
    bool is_open = true;
    ScopeGuard debug_output_guard{NoCreate};
};

} // namespace floormat

namespace floormat::detail {

template<typename F>
requires requires (F&& fn) { requires !std::is_void_v<decltype(forward<F>(fn)())>; }
[[nodiscard]] auto check_glcall(F&& fn, StringView expression, gl_error_policy policy,
                                std::source_location location = std::source_location::current())
{
    using R = decltype(forward<F>(fn)());
    using T = std::conditional_t<std::is_lvalue_reference_v<R>,
                                 Reference<std::remove_reference_t<R>>,
                                 std::remove_cvref_t<R>>;
    Optional<T> result;
    const bool ok = check_glcall([&] { result.emplace(forward<F>(fn)()); }, expression, policy, location);
    return Pair<T, bool>{move(*result), ok};
}

} // namespace floormat::detail

// expression comes stringized from the caller, since passing it on would macro-expand names like glViewport.
#define detail_floormat_glcallʹ(level,expression,...)                                                   \
    ::floormat::detail::check_glcall([&]() -> decltype(auto) { return __VA_ARGS__; },                   \
        ::Corrade::Containers::StringView{expression, sizeof(expression) - 1,                           \
                                          ::Corrade::Containers::StringViewFlag::Global |               \
                                          ::Corrade::Containers::StringViewFlag::NullTerminated},       \
        (level))

#define fm_check_glcall(...)  detail_floormat_glcallʹ(::floormat::gl_error_policy::error, #__VA_ARGS__, __VA_ARGS__)
#define fm_warn_glcall(...)   detail_floormat_glcallʹ(::floormat::gl_error_policy::warn, #__VA_ARGS__, __VA_ARGS__)
#define fm_glcall(...)        detail_floormat_glcallʹ(::floormat::gl_error_policy::ignore, #__VA_ARGS__, __VA_ARGS__)
