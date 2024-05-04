#pragma once
#include "borrowed-ptr-fwd.hpp"

namespace floormat {

template<typename T> constexpr bptr<T>::bptr(NoInitT) noexcept {};
template<typename T> constexpr bptr<T>::bptr(std::nullptr_t) noexcept: ptr{nullptr}, blk{nullptr} {}
template<typename T> constexpr bptr<T>::bptr() noexcept: bptr{nullptr} {}

template<typename T> bptr(T* ptr) -> bptr<T>;

template<typename T, typename U>
CORRADE_ALWAYS_INLINE bptr<U> static_pointer_cast(const bptr<T>& p) noexcept
{
    return p.template static_pointer_cast<U>();
}

} // namespace floormat
