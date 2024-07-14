#pragma once
#include "borrowed-ptr.hpp"
#include "compat/assert.hpp"
#include <utility>

#define FM_BPTR_DEBUG

#ifdef __CLION_IDE__
#define fm_bptr_assert(...) (void(__VA_ARGS__))
#elif defined FM_BPTR_DEBUG && !defined FM_NO_DEBUG
#define fm_bptr_assert(...) fm_assert(__VA_ARGS__)
#else
#define fm_bptr_assert(...) void()
#endif

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace floormat::detail_borrowed_ptr {

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif
struct control_block
{
    bptr_base* _ptr;
    uint32_t _count;
    static void decrement(control_block*& blk) noexcept;
};
#ifdef __GNUG__
#pragma GCC diagnostic pop

#endif

} // namespace floormat::detail_borrowed_ptr

namespace floormat {

template<typename T>
template<typename... Ts>
requires std::is_constructible_v<std::remove_const_t<T>, Ts&&...>
bptr<T>::bptr(InPlaceInitT, Ts&&... args) noexcept:
bptr{ new std::remove_const_t<T>{ forward<Ts...>(args)... } }
{
}

template<typename T> bptr<T>::bptr(std::nullptr_t) noexcept: blk{nullptr} {}
template<typename T> bptr<T>::bptr() noexcept: bptr{nullptr} {}

template<typename T> bptr<T>::bptr(T* ptr) noexcept:
    blk{ptr ? new detail_borrowed_ptr::control_block{const_cast<std::remove_const_t<T>*>(ptr), 1} : nullptr}
{}

template<typename T>
bptr<T>::~bptr() noexcept
{
    if (blk)
        blk->decrement(blk);
}

template<typename T> bptr<T>::bptr(const bptr& other) noexcept: bptr{other, nullptr} {}
template<typename T> bptr<T>& bptr<T>::operator=(const bptr& other) noexcept { return _copy_assign(other); }

template<typename T>
template<detail_borrowed_ptr::DerivedFrom<T> Y>
bptr<T>::bptr(const bptr<Y>& other) noexcept: bptr{other, nullptr} {}

template<typename T>
template<detail_borrowed_ptr::DerivedFrom<T> Y>
bptr<T>& bptr<T>::operator=(const bptr<Y>& other) noexcept
{ return _copy_assign(other); }

template<typename T> bptr<T>& bptr<T>::operator=(bptr&& other) noexcept { return _move_assign(move(other)); }
template<typename T> bptr<T>::bptr(bptr&& other) noexcept: bptr{move(other), nullptr} {}

template<typename T>
template<detail_borrowed_ptr::DerivedFrom<T> Y>
bptr<T>::bptr(bptr<Y>&& other) noexcept: bptr{move(other), nullptr} {}

template<typename T>
template<detail_borrowed_ptr::DerivedFrom<T> Y>
bptr<T>& bptr<T>::operator=(bptr<Y>&& other) noexcept
{ return _move_assign(move(other)); }

template<typename T>
bptr<T>::operator bptr<const T>() const noexcept requires (!std::is_const_v<T>) {
    if (blk && blk->_ptr)
    {
        ++blk->_count;
        return bptr<const T>{};
    }
    return bptr<const T>{nullptr};
}

template<typename T>
void bptr<T>::reset() noexcept
{
    if (blk)
    {
        blk->decrement(blk);
        blk = nullptr;
    }
}

template<typename T>
void bptr<T>::destroy() noexcept
{
    if (!blk)
        return;
    delete blk->_ptr;
    blk->_ptr = nullptr;
}

template<typename T> bptr<T>& bptr<T>::operator=(std::nullptr_t) noexcept { reset(); return *this; }

template<typename T>
template<typename Y>
bptr<T>::bptr(const bptr<Y>& other, std::nullptr_t) noexcept:
    blk{other.blk}
{
    if (blk)
    {
        ++blk->_count;
        fm_bptr_assert(blk->_count > 1);
    }
}

template<typename T>
template<typename Y>
bptr<T>& bptr<T>::_copy_assign(const bptr<Y>& other) noexcept
{
    if (blk != other.blk)
    {
        if (blk)
            blk->decrement(blk);
        blk = other.blk;
        if (blk)
            ++blk->_count;
    }
    return *this;
}

template<typename T>
template<typename Y>
bptr<T>::bptr(bptr<Y>&& other, std::nullptr_t) noexcept:
    blk{other.blk}
{
    other.blk = nullptr;
}

template<typename T>
template<typename Y>
bptr<T>& bptr<T>::_move_assign(bptr<Y>&& other) noexcept
{
    if (blk)
        blk->decrement(blk);
    blk = other.blk;
    other.blk = nullptr;
    return *this;
}

template<typename T> T* bptr<T>::get() const noexcept
{
    if (blk) [[likely]]
        return static_cast<T*>(blk->_ptr);
    else
        return nullptr;
}

template<typename T> T* bptr<T>::operator->() const noexcept
{
    auto* ret = get();
    fm_bptr_assert(ret);
    return ret;
}

template<typename T> T& bptr<T>::operator*() const noexcept { return *operator->(); }

template<typename T> bptr<T>::operator bool() const noexcept { return get(); }
template<typename T> bool bptr<T>::operator==(const bptr<const std::remove_const_t<T>>& other) const noexcept { return get() == other.get(); }
template<typename T> bool bptr<T>::operator==(const bptr<std::remove_const_t<T>>& other) const noexcept { return get() == other.get(); }

template<typename T>
void bptr<T>::swap(bptr& other) noexcept
{
    floormat::swap(blk, other.blk);
}

template<typename T> uint32_t bptr<T>::use_count() const noexcept
{
    if (blk && blk->_ptr) [[likely]]
        return blk->_count;
    else
        return 0;
}

} // namespace floormat

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
