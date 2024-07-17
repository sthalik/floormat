#pragma once
#include "borrowed-ptr.hpp"
#include "compat/assert.hpp"

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace floormat {

template<typename T> bptr<T>::~bptr() noexcept { detail_bptr::control_block::decrement(blk); }

template<typename T> bptr<T>::bptr(const bptr<std::remove_const_t<T>>& ptr) noexcept requires std::is_const_v<T>: bptr{ptr, nullptr} {}
template<typename T> bptr<T>::bptr(bptr<std::remove_const_t<T>>&& ptr) noexcept requires std::is_const_v<T>: bptr{move(ptr), nullptr} {}

template<typename T> bptr<T>::bptr(const bptr& other) noexcept: bptr{other, nullptr} {}
template<typename T> bptr<T>::bptr(bptr&& other) noexcept: bptr{move(other), nullptr} {}
template<typename T> bptr<T>& bptr<T>::operator=(const bptr& other) noexcept { return _copy_assign(other); }
template<typename T> bptr<T>& bptr<T>::operator=(bptr&& other) noexcept { return _move_assign(move(other)); }

template<typename T>
template<detail_bptr::DerivedFrom<T> Y>
bptr<T>::bptr(const bptr<Y>& other) noexcept:
    bptr{other, nullptr}
{}

template<typename T>
template<detail_bptr::DerivedFrom<T> Y>
bptr<T>& bptr<T>::operator=(const bptr<Y>& other) noexcept
{ return _copy_assign(other); }

template<typename T>
template<detail_bptr::DerivedFrom<T> Y>
bptr<T>::bptr(bptr<Y>&& other) noexcept:
    bptr{move(other), nullptr}
{}

template<typename T>
template<detail_bptr::DerivedFrom<T> Y>
bptr<T>& bptr<T>::operator=(bptr<Y>&& other) noexcept
{ return _move_assign(move(other)); }

template<typename T> void bptr<T>::reset() noexcept { detail_bptr::control_block::decrement(blk); }

template<typename T>
template<detail_bptr::DerivedFrom<T> Y>
void bptr<T>::reset(Y* ptr) noexcept
{
    detail_bptr::control_block::decrement(blk);
    blk = ptr ? new detail_bptr::control_block{const_cast<std::remove_const_t<Y>*>(ptr), 1,
#ifndef FM_NO_WEAK_BPTR
        1,
#endif
    } : nullptr;
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
#ifndef FM_NO_WEAK_BPTR
        ++blk->_soft_count;
#endif
        ++blk->_hard_count;
    }
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
bptr<T>& bptr<T>::_copy_assign(const bptr<Y>& other) noexcept
{
    if (blk != other.blk)
    {
        detail_bptr::control_block::decrement(blk);
        blk = other.blk;
        if (blk)
        {
#ifndef FM_NO_WEAK_BPTR
            ++blk->_soft_count;
#endif
            ++blk->_hard_count;
        }
    }
    return *this;
}

template<typename T>
template<typename Y>
bptr<T>& bptr<T>::_move_assign(bptr<Y>&& other) noexcept
{
    detail_bptr::control_block::decrement(blk);
    blk = other.blk;
    other.blk = nullptr;
    return *this;
}

template<typename T>
T* bptr<T>::get() const noexcept
{
    if (blk) [[likely]]
        return static_cast<T*>(blk->_ptr);
    else
        return nullptr;
}

template<typename T>
T* bptr<T>::operator->() const noexcept
{
    auto* ret = get();
    fm_bptr_assert(ret);
    return ret;
}

template<typename T> T& bptr<T>::operator*() const noexcept { return *operator->(); }

template<typename T> bptr<T>::operator bool() const noexcept { return blk && blk->_ptr; }

template<typename T> bool bptr<T>::operator==(const bptr<const T>& other) const noexcept
{
    return blk ? (other.blk && blk->_ptr == other.blk->_ptr) : !other.blk;
}
template<typename T> bool bptr<T>::operator==(const bptr<T>& other) const noexcept requires (!std::is_const_v<T>) {
    return blk ? (other.blk && blk->_ptr == other.blk->_ptr) : !other.blk;
}

template<typename T> bool bptr<T>::operator==(const std::nullptr_t&) const noexcept { return !blk || !blk->_ptr; }

template<typename T>
std::strong_ordering bptr<T>::operator<=>(const bptr<const T>& other) const noexcept
{ return get() <=> other.get(); }

template<typename T> std::strong_ordering bptr<T>::operator<=>(const bptr<T>& other) const noexcept requires (!std::is_const_v<T>)
{ return get() <=> other.get(); }

template<typename T> std::strong_ordering bptr<T>::operator<=>(const std::nullptr_t&) const noexcept
{ return get() <=> (T*)nullptr; }

template<typename T> void bptr<T>::swap(bptr& other) noexcept { floormat::swap(blk, other.blk); }

template<typename T>
uint32_t bptr<T>::use_count() const noexcept
{
    if (blk && blk->_ptr) [[likely]]
        return blk->_hard_count;
    else
        return 0;
}

template<typename To, typename From>
requires detail_bptr::StaticCastable<From, To>
bptr<To> static_pointer_cast(bptr<From>&& p) noexcept
{
    if (p.blk && p.blk->_ptr) [[likely]]
    {
        bptr<To> ret{nullptr};
        ret.blk = p.blk;
        p.blk = nullptr;
        return ret;
    }
    return bptr<To>{nullptr};
}

template<typename To, typename From>
requires detail_bptr::StaticCastable<From, To>
bptr<To> static_pointer_cast(const bptr<From>& p) noexcept
{
    if (p.blk && p.blk->_ptr) [[likely]]
    {
        bptr<To> ret{nullptr};
#ifndef FM_NO_WEAK_BPTR
        ++p.blk->_soft_count;
#endif
        ++p.blk->_hard_count;
        ret.blk = p.blk;
        return ret;
    }
    return bptr<To>{nullptr};
}

} // namespace floormat

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
