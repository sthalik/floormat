#pragma once
#include "borrowed-ptr-fwd.hpp"

namespace floormat {

template<typename T> constexpr bptr<T>::bptr(std::nullptr_t) noexcept: ptr{nullptr}, blk{nullptr} {}
template<typename T> constexpr bptr<T>::bptr() noexcept: bptr{nullptr} {}

template<typename T, typename U>
bptr<U> static_pointer_cast(const bptr<T>& p) noexcept
{
    static_assert(detail_borrowed_ptr::StaticCastable<T, U>);

    if (auto* blk = p.blk) [[likely]]
    {
        auto* ptr = static_cast<U*>(p.ptr);
        blk->incr();
        return bptr<U>{DirectInit, ptr, blk};
    }
    else
        return bptr<U>{nullptr};
}

} // namespace floormat
