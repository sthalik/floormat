#pragma once
#include "function-view-fwd.hpp"
#include "common-function.hpp"
#include "invoke.hpp"
#include "assert.hpp"
#include "defs.hpp"
#include "move.hpp"

namespace floormat::detail::function_view_impl {

template<typename Fn> union bound_entity
{
    const void* obj;
    Fn* fn;
};

template<bool Const, bool Noexcept, typename R, typename... Args>
struct invoker
{
    using fn_type = R(Args...) noexcept(Noexcept);
    using entity = bound_entity<R(Args...)>;
    using thunk = R(entity, Args...) noexcept(Noexcept);

    template<typename T> static constexpr R invoke_object(entity e, Args... args) noexcept(Noexcept)
    {
        T& f = *static_cast<T*>(const_cast<void*>(e.obj));
        if constexpr (std::is_void_v<R>)
            return static_cast<void>(invoke(f, forward<Args>(args)...));
        else
            return invoke(f, forward<Args>(args)...);
    }

    static constexpr R invoke_function(entity e, Args... args) noexcept(Noexcept) { return e.fn(forward<Args>(args)...); }

    [[noreturn]] static R invoke_empty(entity, Args...) noexcept { fm_abort("called an empty function_view"); }
};

template<typename T, bool Noexcept, typename R, typename... Args>
concept invocable_using = (Noexcept ? std::is_nothrow_invocable_r_v<R, T&, Args...> : std::is_invocable_r_v<R, T&, Args...>) &&
                          !common_function_impl::reference_converts_from_temporary<R, std::invoke_result_t<T&, Args...>>;

template<bool Const, bool Noexcept, typename R, typename... Args>
class function_view_base
{
    using invoker_type = invoker<Const, Noexcept, R, Args...>;
    using fn_type = invoker_type::fn_type;
    template<typename F> using target = std::conditional_t<Const, const std::remove_cvref_t<F>, std::remove_reference_t<F>>;

    invoker_type::thunk* _thunk = &invoker_type::invoke_empty;
    invoker_type::entity _bound{.obj = nullptr};

public:
    constexpr function_view_base() noexcept = default;
    constexpr function_view_base(decltype(nullptr)) noexcept {}

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-lifetimebound-violation")
#pragma clang diagnostic ignored "-Wlifetime-safety-lifetimebound-violation"
#endif
#endif
    template<typename F>
    requires (!std::is_base_of_v<function_view_base, std::remove_cvref_t<F>> &&
              !std::is_function_v<std::remove_reference_t<F>> &&
              !std::is_volatile_v<std::remove_reference_t<F>> &&
              invocable_using<target<F>, Noexcept, R, Args...>)
    constexpr function_view_base(F&& f [[fm_lifetimebound]]) noexcept:
        _thunk{&invoker_type::template invoke_object<target<F>>}, _bound{.obj = __builtin_addressof(f)}
    {
        if (common_function_impl::is_empty_callable(f))
            _thunk = &invoker_type::invoke_empty;
    }
#ifdef __clang__
#pragma clang diagnostic pop
#endif

    constexpr function_view_base(fn_type* fn) noexcept: _thunk{&invoker_type::invoke_function}, _bound{.fn = fn}
    {
        if (common_function_impl::pointers_equal(fn, nullptr))
            _thunk = &invoker_type::invoke_empty;
    }

    constexpr R operator()(Args... args) const noexcept(Noexcept) { return _thunk(_bound, forward<Args>(args)...); }

    constexpr explicit operator bool() const noexcept { return !common_function_impl::pointers_equal(_thunk, &invoker_type::invoke_empty); }
    friend constexpr bool operator==(const function_view_base& f, decltype(nullptr)) noexcept { return !f; }
};

} // namespace floormat::detail::function_view_impl

namespace floormat {

template<typename R, typename... Args, bool Noexcept>
class function_view<R(Args...) noexcept(Noexcept)> final:
    public detail::function_view_impl::function_view_base<false, Noexcept, R, Args...>
{
    using detail::function_view_impl::function_view_base<false, Noexcept, R, Args...>::function_view_base;
};

template<typename R, typename... Args, bool Noexcept>
class function_view<R(Args...) const noexcept(Noexcept)> final:
    public detail::function_view_impl::function_view_base<true, Noexcept, R, Args...>
{
    using detail::function_view_impl::function_view_base<true, Noexcept, R, Args...>::function_view_base;
};

} // namespace floormat
