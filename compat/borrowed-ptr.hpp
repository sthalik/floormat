#pragma once
#include "borrowed-ptr-fwd.hpp"
#include "borrowed-ptr.inl"

#define FM_BPTR_DEBUG
#ifdef FM_BPTR_DEBUG
#include "compat/assert.hpp"
#define fm_bptr_assert(...) fm_debug_assert(__VA_ARGS__)
#else
#define fm_bptr_assert(...) void()
#endif

namespace floormat::detail_borrowed_ptr {

//static_assert(std::is_same_v<T, U> || std::has_virtual_destructor_v<T> && std::has_virtual_destructor_v<T>); // todo! for simple_bptr

template<typename From, typename To>
concept StaticCastable = requires(From* from) {
    static_cast<To*>(from);
};

} // namespace floormat::detail_borrowed_ptr

namespace floormat {

template<typename T> constexpr bptr<T>::bptr(std::nullptr_t) noexcept: ptr{nullptr}, blk{nullptr} {}
template<typename T> constexpr bptr<T>::bptr() noexcept: bptr{nullptr} {}

template<typename T> constexpr T* bptr<T>::get() const noexcept { return ptr; }
template<typename T> constexpr T& bptr<T>::operator*() const noexcept { fm_debug_assert(ptr); return *ptr; }
template<typename T> constexpr T* bptr<T>::operator->() const noexcept { fm_debug_assert(ptr); return ptr; }

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
