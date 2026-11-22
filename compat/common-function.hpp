#pragma once
#include <type_traits>
#include <utility>

namespace floormat::detail::common_function_impl {

#ifdef __cpp_lib_reference_from_temporary
template<typename R, typename T>
constexpr inline bool reference_converts_from_temporary = std::reference_converts_from_temporary_v<R, T>;
#else
// libc++ 19 lacks the trait. Later libc++ has it but doesn't define the feature macro.
template<typename R, typename T>
constexpr inline bool reference_converts_from_temporary = __reference_converts_from_temporary(R, T);
#endif

// Under UBSan, gcc can't fold a comparison of a function's address to another function's or to
// null in constant evaluation (PR 71962). The ones it can't fold are false.
template<typename P, typename Q> constexpr bool pointers_equal(P a, Q b) noexcept
{
#if defined __GNUC__ && !defined __clang__
    if consteval
    {
        if (!__builtin_constant_p(a == b))
            return false;
    }
#endif
    return a == b;
}

// A callable that converts to false, such as a null pointer or an empty function, leaves the wrapper empty.
template<typename F> constexpr bool is_empty_callable(F& f)
{
    if constexpr (std::is_pointer_v<std::remove_cv_t<F>> || std::is_member_pointer_v<std::remove_cv_t<F>>)
        return pointers_equal(f, nullptr);
    // A captureless lambda, through its function pointer.
    else if constexpr (std::is_class_v<F> && requires (F& g) { requires std::is_pointer_v<decltype(+g)>; })
        return pointers_equal(+f, nullptr);
    else if constexpr (std::is_class_v<F> && requires (F& g) { static_cast<bool>(g); })
        return !static_cast<bool>(f);
    else
        return false;
}

} // namespace floormat::detail::common_function_impl
