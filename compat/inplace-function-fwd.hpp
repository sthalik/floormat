#pragma once
#include "compat/integer-types.hpp"

// inplace-function.hpp uses it too and #undefs it.
#define FM_INPLACE_FUNCTION_QUALIFIERS(X)              \
    X(, , false, false, none)                          \
    X(const, , true, false, none)                      \
    X(volatile, , false, true, none)                   \
    X(const volatile, , true, true, none)              \
    X(, &, false, false, lvalue)                       \
    X(const, &, true, false, lvalue)                   \
    X(volatile, &, false, true, lvalue)                \
    X(const volatile, &, true, true, lvalue)           \
    X(, &&, false, false, rvalue)                      \
    X(const, &&, true, false, rvalue)                  \
    X(volatile, &&, false, true, rvalue)               \
    X(const volatile, &&, true, true, rvalue)

namespace floormat::detail::inplace_function_impl {

enum class ref_qualifier : uint8_t { none, lvalue, rvalue };

template<bool Copyable, bool Const, bool Volatile, ref_qualifier Ref, bool Noexcept, size_t Capacity, size_t Alignment, typename R, typename... Args>
class function;

template<typename Signature> struct signature;

#define FM_INPLACE_FUNCTION_SIGNATURE(CV, REF, IS_CONST, IS_VOLATILE, REF_KIND)                     \
    template<typename R, typename... Args> struct signature<R(Args...) CV REF>                      \
    {                                                                                               \
        template<bool Copyable, size_t Capacity, size_t Alignment>                                  \
        using type = function<Copyable, IS_CONST, IS_VOLATILE, ref_qualifier::REF_KIND, false,      \
                              Capacity, Alignment, R, Args...>;                                     \
    };                                                                                              \
    template<typename R, typename... Args> struct signature<R(Args...) CV REF noexcept>             \
    {                                                                                               \
        template<bool Copyable, size_t Capacity, size_t Alignment>                                  \
        using type = function<Copyable, IS_CONST, IS_VOLATILE, ref_qualifier::REF_KIND, true,       \
                              Capacity, Alignment, R, Args...>;                                     \
    };
FM_INPLACE_FUNCTION_QUALIFIERS(FM_INPLACE_FUNCTION_SIGNATURE)
#undef FM_INPLACE_FUNCTION_SIGNATURE

} // namespace floormat::detail::inplace_function_impl

namespace floormat {

// A hot caller can declare an inplace_function whose sizeof is at most 64 as alignas(64), so all of it sits
// in one cache line and no access to it splits a line or a page. A local pays with a realigned stack frame.
template<typename Signature, size_t Capacity, size_t Alignment = alignof(void*)>
using inplace_function = typename detail::inplace_function_impl::signature<Signature>::template type<false, Capacity, Alignment>;

template<typename Signature, size_t Capacity, size_t Alignment = alignof(void*)>
using inplace_copyable_function = typename detail::inplace_function_impl::signature<Signature>::template type<true, Capacity, Alignment>;

} // namespace floormat
