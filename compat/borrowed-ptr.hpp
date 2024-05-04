#pragma once
#include "borrowed-ptr-fwd.hpp"
#include "borrowed-ptr.inl"
#include "borrowed-ptr.inl"

#define FM_BPTR_DEBUG

#ifdef FM_NO_DEBUG
#undef FM_BPTR_DEBUG
#endif
#ifdef FM_BPTR_DEBUG
#include "compat/assert.hpp"
#define fm_bptr_assert(...) fm_assert(__VA_ARGS__)
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

template<typename U, typename T>
bptr<U> static_pointer_cast(const bptr<T>& p) noexcept
{
    // hack to generate better error message
    if constexpr (detail_borrowed_ptr::StaticCastable<T, U>)
    {
        if (p.blk && p.blk->_ptr) [[likely]]
        {
            fm_bptr_assert(p.casted_ptr);
            auto* ret = static_cast<U*>(p.casted_ptr);
            return bptr<U>{DirectInit, ret, p.blk};
        }
    }
    else
        // concepts can't be forward-declared so use static_assert
        static_assert(detail_borrowed_ptr::StaticCastable<T, U>);

    return bptr<U>{nullptr};
}

} // namespace floormat
