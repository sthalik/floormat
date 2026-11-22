#pragma once
#include <type_traits>
#include <utility>
#include <cr/Macros.h>

namespace floormat::detail::invoke_impl {

template<typename T> constexpr inline bool is_reference_wrapper = !std::is_same_v<std::unwrap_reference_t<T>, T>;

// MSVC 19.52 can't constant-evaluate a write through `call().*f`, so the object comes in as a
// parameter.
template<typename M, typename C, typename O, typename... Args>
constexpr CORRADE_ALWAYS_INLINE decltype(auto) invoke_member_of(M C::* f, O&& object, Args&&... args)
{
    if constexpr (std::is_function_v<M>)
        return (static_cast<O&&>(object).*f)(std::forward<Args>(args)...);
    else
        return static_cast<O&&>(object).*f;
}

template<typename M, typename C, typename T, typename... Args>
constexpr CORRADE_ALWAYS_INLINE decltype(auto) invoke_member(M C::* f, T&& t, Args&&... args)
{
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_base_of_v<C, U>)
        return invoke_member_of(f, static_cast<T&&>(t), std::forward<Args>(args)...);
    else if constexpr (is_reference_wrapper<U>)
        return invoke_member_of(f, t.get(), std::forward<Args>(args)...);
    else
        return invoke_member_of(f, *static_cast<T&&>(t), std::forward<Args>(args)...);
}

} // namespace floormat::detail::invoke_impl

namespace floormat::detail {

struct invoke_fn
{
    template<typename F, typename... Args> requires std::is_invocable_v<F, Args...>
    static constexpr CORRADE_ALWAYS_INLINE decltype(auto) operator()(F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>);
};

template<typename F, typename... Args> requires std::is_invocable_v<F, Args...>
constexpr decltype(auto) invoke_fn::operator()(F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
{
    if constexpr (std::is_member_pointer_v<std::remove_cvref_t<F>>)
        return invoke_impl::invoke_member(f, std::forward<Args>(args)...);
    else
        return std::forward<F>(f)(std::forward<Args>(args)...);
}

} // namespace floormat::detail

namespace floormat {

// std::invoke without <functional>.
constexpr inline detail::invoke_fn invoke{};

} // namespace floormat
