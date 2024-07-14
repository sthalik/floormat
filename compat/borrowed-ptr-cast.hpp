#pragma once
#include "borrowed-ptr-fwd.hpp"
#include "compat/assert.hpp"

namespace floormat::detail_borrowed_ptr {

//static_assert(std::is_same_v<T, U> || std::has_virtual_destructor_v<T> && std::has_virtual_destructor_v<T>); // todo! for simple_bptr

template<typename From, typename To>
concept StaticCastable = requires(From* from) {
    static_cast<To*>(from);
};

} // namespace floormat::detail_borrowed_ptr

namespace floormat {

template<typename To, typename From>
bptr<To> static_pointer_cast(const bptr<From>& p) noexcept
{
    // hack to generate better error message
    if constexpr (detail_borrowed_ptr::StaticCastable<From, To>)
    {
        if (p.blk && p.blk->_ptr) [[likely]]
            return bptr<To>{p, nullptr};
    }
    else
    {
        using detail_borrowed_ptr::StaticCastable;
        // concepts can't be forward-declared so use static_assert
        static_assert(StaticCastable<From, To>,
            "cannot static_cast, classes must be related by inheritance");
    }

    return bptr<To>{nullptr};
}

} // namespace floormat
