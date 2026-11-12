#pragma once
#include "integer-types.hpp"

namespace floormat::detail::inplace_function_impl {

enum class ref_qualifier : u8 { none, lvalue, rvalue };

template<bool Copyable, bool Const, bool Volatile, ref_qualifier Ref, bool Noexcept, usize Capacity, usize Alignment, typename R, typename... Args>
class function_base;

} // namespace floormat::detail::inplace_function_impl

namespace floormat {

// A hot caller can declare an inplace_function whose sizeof is at most 64 as alignas(64), so all of it sits
// in one cache line and no access to it splits a line or a page. A local pays with a realigned stack frame.
template<typename Signature, usize Capacity, usize Alignment = alignof(void*)> class inplace_function;

template<typename Signature, usize Capacity, usize Alignment = alignof(void*)> class inplace_copyable_function;

} // namespace floormat
